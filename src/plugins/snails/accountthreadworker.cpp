/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "accountthreadworker.h"
#include <algorithm>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <QMutexLocker>
#include <QUrl>
#include <QFile>
#include <QSslSocket>
#include <QtDebug>
#include <QTimer>
#include <vmime/security/defaultAuthenticator.hpp>
#include <vmime/security/cert/defaultCertificateVerifier.hpp>
#include <vmime/security/cert/X509Certificate.hpp>
#include <vmime/net/transport.hpp>
#include <vmime/net/store.hpp>
#include <vmime/net/message.hpp>
#include <vmime/utility/datetimeUtils.hpp>
#include <vmime/dateTime.hpp>
#include <vmime/messageParser.hpp>
#include <vmime/messageBuilder.hpp>
#include <vmime/htmlTextPart.hpp>
#include <vmime/stringContentHandler.hpp>
#include <vmime/fileAttachment.hpp>
#include <vmime/messageIdSequence.hpp>
#include <util/util.h>
#include <util/xpc/util.h>
#include "message.h"
#include "account.h"
#include "core.h"
#include "progresslistener.h"
#include "storage.h"
#include "vmimeconversions.h"
#include "outputiodevadapter.h"
#include "common.h"
#include "messagechangelistener.h"
#include "folder.h"
#include "tracerfactory.h"

namespace LeechCraft
{
namespace Snails
{
	namespace
	{
		class VMimeAuth : public vmime::security::defaultAuthenticator
		{
			Account::Direction Dir_;
			Account *Acc_;
		public:
			VMimeAuth (Account::Direction, Account*);

			const vmime::string getUsername () const;
			const vmime::string getPassword () const;
		private:
			QByteArray GetID () const
			{
				QByteArray id = "org.LeechCraft.Snails.PassForAccount/" + Acc_->GetID ();
				id += Dir_ == Account::Direction::Out ? "/Out" : "/In";
				return id;
			}
		};

		VMimeAuth::VMimeAuth (Account::Direction dir, Account *acc)
		: Dir_ (dir)
		, Acc_ (acc)
		{
		}

		const vmime::string VMimeAuth::getUsername () const
		{
			switch (Dir_)
			{
			case Account::Direction::Out:
				return Acc_->GetOutUsername ().toUtf8 ().constData ();
			default:
				return Acc_->GetInUsername ().toUtf8 ().constData ();
			}
		}

		const vmime::string VMimeAuth::getPassword () const
		{
			QString pass;

			QMetaObject::invokeMethod (Acc_,
					"getPassword",
					Qt::BlockingQueuedConnection,
					Q_ARG (QString*, &pass));

			return pass.toUtf8 ().constData ();
		}

		template<typename Function, typename... Types>
		std::result_of_t<Function (AccountThreadWorker*, Types...)>* InvokeWith (const std::tuple<Types...>&, int)
		{
			return nullptr;
		}

		template<typename T>
		struct AlwaysFalse : public std::false_type {};

		template<typename Function, typename... Types>
		void* InvokeWith (const std::tuple<Types...>&, ...)
		{
			static_assert (AlwaysFalse<Function>::value, "Check your function signature and arguments.");
			return nullptr;
		}

		struct ArgChecker
		{
			ArgChecker ()
			{
				boost::fusion::for_each (AccountThreadWorker::Args::Known {}, *this);
			}

			template<typename T>
			void operator() (const T&) const
			{
				InvokeWith<typename T::Function> (typename T::ArgTypes {}, 0);
			}
		};
	}

	AccountThreadWorker::AccountThreadWorker (bool isListening,
			const QString& threadName, Account *parent)
	: A_ (parent)
	, NoopTimer_ (new QTimer (this))
	, IsListening_ (isListening)
	, ThreadName_ (threadName)
	, ChangeListener_ (new MessageChangeListener (this))
	, Session_ (vmime::make_shared<vmime::net::session> ())
	, CachedFolders_ (2)
	, CertVerifier_ (vmime::make_shared<vmime::security::cert::defaultCertificateVerifier> ())
	, InAuth_ (vmime::make_shared<VMimeAuth> (Account::Direction::In, A_))
	{
		std::vector<boost::shared_ptr<vmime::security::cert::X509Certificate>> vCerts;
		for (const auto& sysCert : QSslSocket::systemCaCertificates ())
		{
			const auto& der = sysCert.toDer ();
			const auto bytes = reinterpret_cast<const vmime::byte_t*> (der.constData ());
			vCerts.push_back (vmime::security::cert::X509Certificate::import (bytes, der.size ()));
		}
		CertVerifier_->setX509RootCAs (vCerts);

		if (IsListening_)
			connect (ChangeListener_,
					SIGNAL (messagesChanged (QStringList, QList<int>)),
					this,
					SLOT (handleMessagesChanged (QStringList, QList<int>)));

		connect (NoopTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (sendNoop ()));
		NoopTimer_->start (60 * 1000);
	}

	vmime::shared_ptr<vmime::net::store> AccountThreadWorker::MakeStore ()
	{
		if (CachedStore_)
			return CachedStore_;

		QString url;

		QMetaObject::invokeMethod (A_,
				"buildInURL",
				Qt::BlockingQueuedConnection,
				Q_ARG (QString*, &url));

		auto st = Session_->getStore (vmime::utility::url (url.toUtf8 ().constData ()));
		st->setTracerFactory (vmime::make_shared<TracerFactory> (ThreadName_, A_->GetLogger ()));
		st->setCertificateVerifier (CertVerifier_);
		st->setAuthenticator (InAuth_);

		if (A_->UseTLS_)
		{
			st->setProperty ("connection.tls", A_->UseTLS_);
			st->setProperty ("connection.tls.required", A_->InSecurityRequired_);
		}
		st->setProperty ("options.sasl", A_->UseSASL_);
		st->setProperty ("options.sasl.fallback", A_->SASLRequired_);
		st->setProperty ("server.port", A_->InPort_);

		CachedStore_ = st;

		st->connect ();

		if (IsListening_)
			if (const auto defFolder = st->getDefaultFolder ())
			{
				defFolder->addMessageChangedListener (ChangeListener_);
				CachedFolders_ [GetFolderPath (defFolder)] = defFolder;
			}

		return st;
	}

	vmime::shared_ptr<vmime::net::transport> AccountThreadWorker::MakeTransport ()
	{
		QString url;

		QMetaObject::invokeMethod (A_,
				"buildOutURL",
				Qt::BlockingQueuedConnection,
				Q_ARG (QString*, &url));

		QString username;
		QString password;
		bool setAuth = false;
		if (A_->SMTPNeedsAuth_ &&
				A_->OutType_ == Account::OutType::SMTP)
		{
			setAuth = true;

			QUrl parsed = QUrl::fromEncoded (url.toUtf8 ());
			username = parsed.userName ();
			password = parsed.password ();

			parsed.setUserName (QString ());
			parsed.setPassword (QString ());
			url = QString::fromUtf8 (parsed.toEncoded ());
			qDebug () << Q_FUNC_INFO << url << username << password;
		}

		auto trp = Session_->getTransport (vmime::utility::url (url.toUtf8 ().constData ()));
		trp->setCertificateVerifier (CertVerifier_);

		if (setAuth)
		{
			trp->setProperty ("options.need-authentication", true);
			trp->setProperty ("auth.username", username.toUtf8 ().constData ());
			trp->setProperty ("auth.password", password.toUtf8 ().constData ());
		}
		trp->setProperty ("server.port", A_->OutPort_);

		if (A_->OutSecurity_ == Account::SecurityType::TLS)
		{
			trp->setProperty ("connection.tls", true);
			trp->setProperty ("connection.tls.required", A_->OutSecurityRequired_);
		}

		return trp;
	}

	namespace
	{
		vmime::net::folder::path Folder2Path (const QStringList& folder)
		{
			if (folder.isEmpty ())
				return vmime::net::folder::path ("INBOX");

			vmime::net::folder::path path;
			for (const auto& comp : folder)
				path.appendComponent ({ comp.toUtf8 ().constData (), vmime::charsets::UTF_8 });
			return path;
		}
	}

	VmimeFolder_ptr AccountThreadWorker::GetFolder (const QStringList& path, FolderMode mode)
	{
		if (path.size () == 1 && path.at (0) == "[Gmail]")
			return {};

		if (!CachedFolders_.contains (path))
		{
			auto store = MakeStore ();
			CachedFolders_ [path] = store->getFolder (Folder2Path (path));
		}

		auto folder = CachedFolders_ [path];

		if (mode == FolderMode::NoChange)
			return folder;

		const auto requestedMode = mode == FolderMode::ReadOnly ?
				vmime::net::folder::MODE_READ_ONLY :
				vmime::net::folder::MODE_READ_WRITE;
		if (folder->isOpen () && folder->getMode () != requestedMode)
			folder->close (false);
		if (!folder->isOpen ())
			folder->open (requestedMode);
		return folder;
	}

	namespace
	{
		class MessageStructHandler
		{
			const Message_ptr Msg_;
			const vmime::shared_ptr<vmime::net::message> VmimeMessage_;
		public:
			MessageStructHandler (const Message_ptr&, const vmime::shared_ptr<vmime::net::message>&);

			void operator() ();
		private:
			void HandlePart (const vmime::shared_ptr<vmime::net::messagePart>&);

			template<typename T>
			void EnumParts (const T&);
		};

		MessageStructHandler::MessageStructHandler (const Message_ptr& msg,
				const vmime::shared_ptr<vmime::net::message>& message)
		: Msg_ { msg }
		, VmimeMessage_ { message }
		{
		}

		void MessageStructHandler::operator() ()
		{
			const auto& structure = VmimeMessage_->getStructure ();
			if (!structure)
				return;

			EnumParts (structure);
		}

		void MessageStructHandler::HandlePart (const boost::shared_ptr<vmime::net::messagePart>& part)
		{
			const auto& type = part->getType ();

			if (type.getType () == "text")
				return;

			if (type.getType () == "multipart")
			{
				EnumParts (part);
				return;
			}

			Msg_->AddAttachment ({
						{},
						{},
						type.getType ().c_str (),
						type.getSubType ().c_str (),
						static_cast<qlonglong> (part->getSize ())
					});
		}

		template<typename T>
		void MessageStructHandler::EnumParts (const T& enumable)
		{
			const auto pc = enumable->getPartCount ();
			for (vmime::size_t i = 0; i < pc; ++i)
				HandlePart (enumable->getPartAt (i));
		}
	}

	Message_ptr AccountThreadWorker::FromHeaders (const vmime::shared_ptr<vmime::net::message>& message) const
	{
		const auto& utf8cs = vmime::charset { vmime::charsets::UTF_8 };

		Message_ptr msg (new Message);
		msg->SetVmimeHeader (message->getHeader ());
		msg->SetFolderID (static_cast<vmime::string> (message->getUID ()).c_str ());
		msg->SetSize (message->getSize ());

		if (message->getFlags () & vmime::net::message::FLAG_SEEN)
			msg->SetRead (true);

		auto header = message->getHeader ();
		try
		{
			if (const auto& from = header->From ())
			{
				const auto& mboxVal = from->getValue ();
				const auto& mbox = vmime::dynamicCast<const vmime::mailbox> (mboxVal);
				msg->AddAddress (Message::Address::From, Mailbox2Strings (mbox));
			}
			else
				qWarning () << "no 'from' data";
		}
		catch (const vmime::exceptions::no_such_field&)
		{
			qWarning () << "no 'from' data";
		}

		try
		{
			if (const auto& replyToHeader = header->ReplyTo ())
			{
				const auto& replyToVal = replyToHeader->getValue ();
				const auto& replyTo = vmime::dynamicCast<const vmime::mailbox> (replyToVal);
				msg->AddAddress (Message::Address::ReplyTo, Mailbox2Strings (replyTo));
			}
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		try
		{
			if (const auto& msgIdField = header->MessageId ())
			{
				const auto& msgIdVal = msgIdField->getValue ();
				const auto& msgId = vmime::dynamicCast<const vmime::messageId> (msgIdVal);
				msg->SetMessageID (msgId->getId ().c_str ());
			}
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		try
		{
			if (const auto& refHeader = header->References ())
			{
				const auto& seqVal = refHeader->getValue ();
				const auto& seq = vmime::dynamicCast<const vmime::messageIdSequence> (seqVal);

				for (const auto& id : seq->getMessageIdList ())
					msg->AddReferences (id->getId ().c_str ());
			}
		}
		catch (const std::exception& e)
		{
			qWarning () << e.what ();
		}

		try
		{
			if (const auto& irt = header->InReplyTo ())
			{
				const auto& seqVal = irt->getValue ();
				const auto& seq = vmime::dynamicCast<const vmime::messageIdSequence> (seqVal);

				for (const auto& id : seq->getMessageIdList ())
					msg->AddInReplyTo (id->getId ().c_str ());
			}
		}
		catch (const std::exception& e)
		{
			qWarning () << e.what ();
		}

		auto setAddresses = [&msg] (Message::Address type,
				const vmime::shared_ptr<const vmime::headerField>& field) -> void
		{
			if (!field)
				return;

			if (const auto& alist = vmime::dynamicCast<const vmime::addressList> (field->getValue ()))
			{
				const auto& vec = alist->toMailboxList ()->getMailboxList ();

				Message::Addresses_t addrs;
				std::transform (vec.begin (), vec.end (), std::back_inserter (addrs),
						[] (decltype (vec.front ()) add) { return Mailbox2Strings (add); });
				msg->SetAddresses (type, addrs);
			}
			else
			{
				const auto fieldPtr = field.get ();
				qWarning () << "no"
						<< static_cast<int> (type)
						<< "data: cannot cast to mailbox list"
						<< typeid (*fieldPtr).name ();
			}
		};

		try
		{
			setAddresses (Message::Address::To, header->To ());
		}
		catch (const vmime::exceptions::no_such_field& nsf)
		{
			qWarning () << "no 'to' data" << nsf.what ();
		}

		try
		{
			setAddresses (Message::Address::Cc, header->Cc ());
		}
		catch (const vmime::exceptions::no_such_field& nsf)
		{
		}

		try
		{
			setAddresses (Message::Address::Bcc, header->Bcc ());
		}
		catch (const vmime::exceptions::no_such_field& nsf)
		{
		}

		try
		{
			if (const auto dateHeader = header->Date ())
			{
				const auto& origDateVal = dateHeader->getValue ();
				const auto& origDate = vmime::dynamicCast<const vmime::datetime> (origDateVal);
				const auto& date = vmime::utility::datetimeUtils::toUniversalTime (*origDate);
				QDate qdate (date.getYear (), date.getMonth (), date.getDay ());
				QTime time (date.getHour (), date.getMinute (), date.getSecond ());
				msg->SetDate (QDateTime (qdate, time, Qt::UTC));
			}
			else
				qWarning () << "no 'date' data";
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		try
		{
			if (const auto subjectHeader = header->Subject ())
			{
				const auto& strVal = subjectHeader->getValue ();
				const auto& str = vmime::dynamicCast<const vmime::text> (strVal);
				msg->SetSubject (QString::fromUtf8 (str->getConvertedText (utf8cs).c_str ()));
			}
			else
				qWarning () << "no 'subject' data";
		}
		catch (const vmime::exceptions::no_such_field&)
		{
		}

		MessageStructHandler { msg, message } ();

		return msg;
	}

	namespace
	{
		vmime::shared_ptr<vmime::message> FromNetMessage (vmime::shared_ptr<vmime::net::message> msg)
		{
			return msg->getParsedMessage ();
		}
	}

	void AccountThreadWorker::FetchMessagesIMAP (const QList<QStringList>& origFolders,
			const QByteArray& last)
	{
		for (const auto& folder : origFolders)
			if (const auto& netFolder = GetFolder (folder, FolderMode::ReadWrite))
				FetchMessagesInFolder (folder, netFolder, last);
	}

	namespace
	{
		MessageVector_t GetMessagesInFolder (const VmimeFolder_ptr& folder, const QByteArray& lastId)
		{
			if (lastId.isEmpty ())
			{
				const auto count = folder->getMessageCount ();

				MessageVector_t messages;
				messages.reserve (count);

				const auto chunkSize = 100;
				for (int i = 0; i < count; i += chunkSize)
				{
					const auto endVal = i + chunkSize;
					const auto& set = vmime::net::messageSet::byNumber (i + 1, std::min (count, endVal));
					try
					{
						const auto& theseMessages = folder->getMessages (set);
						std::move (theseMessages.begin (), theseMessages.end (), std::back_inserter (messages));
					}
					catch (const std::exception& e)
					{
						qWarning () << Q_FUNC_INFO
								<< "cannot get messages from"
								<< i + 1
								<< "to"
								<< endVal
								<< "because:"
								<< e.what ();
						return {};
					}
				}

				return messages;
			}
			else
			{
				const auto& set = vmime::net::messageSet::byUID (lastId.constData (), "*");
				try
				{
					return folder->getMessages (set);
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot get messages from"
							<< lastId
							<< "because:"
							<< e.what ();
					return {};
				}
			}
		}
	}

	QList<Message_ptr> AccountThreadWorker::FetchVmimeMessages (MessageVector_t messages,
			const VmimeFolder_ptr& folder, const QStringList& folderName)
	{
		if (!messages.size ())
			return {};

		const int desiredFlags = vmime::net::fetchAttributes::FLAGS |
					vmime::net::fetchAttributes::SIZE |
					vmime::net::fetchAttributes::UID |
					vmime::net::fetchAttributes::FULL_HEADER |
					vmime::net::fetchAttributes::STRUCTURE |
					vmime::net::fetchAttributes::ENVELOPE;

		try
		{
			const auto& context = tr ("Fetching headers for %1")
					.arg (A_->GetName ());

			folder->fetchMessages (messages, desiredFlags, MkPgListener (context));
		}
		catch (const vmime::exceptions::operation_not_supported& ons)
		{
			qWarning () << Q_FUNC_INFO
					<< "fetch operation not supported:"
					<< ons.what ();
			return {};
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "generally something bad happened:"
					<< e.what ();
			return {};
		}

		QList<Message_ptr> newMessages;
		std::transform (messages.begin (), messages.end (), std::back_inserter (newMessages),
				[this, &folderName] (decltype (messages.front ()) msg)
				{
					auto res = FromHeaders (msg);
					res->AddFolder (folderName);
					return res;
				});
		return newMessages;
	}

	void AccountThreadWorker::FetchMessagesInFolder (const QStringList& folderName,
			const VmimeFolder_ptr& folder, const QByteArray& lastId)
	{
		const auto& changeGuard = ChangeListener_->Disable ();
		Q_UNUSED (changeGuard)

		const std::shared_ptr<void> syncFinishedGuard
		{
			nullptr,
			[this, &folderName, &lastId] (void*) { emit folderSyncFinished (folderName, lastId); }
		};

		qDebug () << Q_FUNC_INFO << folderName << folder.get () << lastId;

		auto messages = GetMessagesInFolder (folder, lastId);
		auto newMessages = FetchVmimeMessages (messages, folder, folderName);
		auto existing = Core::Instance ().GetStorage ()->LoadIDs (A_, folderName);

		QList<QByteArray> ids;

		QList<Message_ptr> updatedMessages;
		Q_FOREACH (const auto& msg, newMessages)
		{
			if (!existing.contains (msg->GetFolderID ()))
				continue;

			existing.removeAll (msg->GetFolderID ());
			newMessages.removeAll (msg);

			bool isUpdated = false;

			auto updated = Core::Instance ().GetStorage ()->LoadMessage (A_, folderName, msg->GetFolderID ());

			if (updated->IsRead () != msg->IsRead ())
			{
				updated->SetRead (msg->IsRead ());
				isUpdated = true;
			}

			auto sumFolders = updated->GetFolders ();
			if (!folderName.isEmpty () &&
					!sumFolders.contains (folderName))
			{
				updated->AddFolder (folderName);
				isUpdated = true;
			}

			updated->SetVmimeHeader (msg->GetVmimeHeader ());

			if (isUpdated)
				updatedMessages << updated;
			else
				ids << msg->GetFolderID ();
		}

		if (ids.size ())
			emit gotOtherMessages (ids, folderName);

		emit gotMsgHeaders (newMessages, folderName);
		emit gotUpdatedMessages (updatedMessages, folderName);

		if (lastId.isEmpty ())
			emit gotMessagesRemoved (existing, folderName);
	}

	namespace
	{
		void FullifyHeaderMessage (const Message_ptr& msg, const vmime::shared_ptr<vmime::net::message>& full)
		{
			vmime::messageParser mp { FromNetMessage (full) };

			if (!msg->GetVmimeHeader ())
				msg->SetVmimeHeader (full->getHeader ());

			QString html;
			QStringList plainParts;
			QStringList htmlPlainParts;

			for (const auto& tp : mp.getTextPartList ())
			{
				const auto& type = tp->getType ();
				if (type.getType () != vmime::mediaTypes::TEXT)
				{
					qWarning () << Q_FUNC_INFO
							<< "non-text in text part"
							<< tp->getType ().getType ().c_str ();
					continue;
				}

				if (type.getSubType () == vmime::mediaTypes::TEXT_HTML)
				{
					auto htp = vmime::dynamicCast<const vmime::htmlTextPart> (tp);
					html = Stringize (htp->getText (), htp->getCharset ());
					htmlPlainParts << Stringize (htp->getPlainText (), htp->getCharset ());
				}
				else if (type.getSubType () == vmime::mediaTypes::TEXT_PLAIN)
					plainParts << Stringize (tp->getText (), tp->getCharset ());
			}

			if (plainParts.isEmpty ())
				plainParts = htmlPlainParts;

			if (std::adjacent_find (plainParts.begin (), plainParts.end (),
					[] (const QString& left, const QString& right)
						{ return left.size () > right.size (); }) == plainParts.end ())
				std::reverse (plainParts.begin (), plainParts.end ());

			msg->SetBody (plainParts.join ("\n"));
			msg->SetHTMLBody (html);

			msg->SetAttachmentList ({});

			for (const auto& att : mp.getAttachmentList ())
			{
				const auto& type = att->getType ();
				if (type.getType () == vmime::mediaTypes::TEXT &&
						(type.getSubType () == vmime::mediaTypes::TEXT_HTML ||
						 type.getSubType () == vmime::mediaTypes::TEXT_PLAIN))
					continue;

				msg->AddAttachment (att);
			}
		}

		FolderType ToFolderType (int specialUse)
		{
			switch (static_cast<vmime::net::folderAttributes::SpecialUses> (specialUse))
			{
			case vmime::net::folderAttributes::SPECIALUSE_SENT:
				return FolderType::Sent;
			case vmime::net::folderAttributes::SPECIALUSE_DRAFTS:
				return FolderType::Drafts;
			case vmime::net::folderAttributes::SPECIALUSE_IMPORTANT:
				return FolderType::Important;
			case vmime::net::folderAttributes::SPECIALUSE_JUNK:
				return FolderType::Junk;
			case vmime::net::folderAttributes::SPECIALUSE_TRASH:
				return FolderType::Trash;
			default:
				return FolderType::Other;
			}
		}
	}

	void AccountThreadWorker::SyncIMAPFolders (vmime::shared_ptr<vmime::net::store> store)
	{
		const auto& root = store->getRootFolder ();
		const auto& inbox = store->getDefaultFolder ();

		QList<Folder> folders;
		for (const auto& folder : root->getFolders (true))
		{
			const auto& attrs = folder->getAttributes ();
			folders.append ({
					GetFolderPath (folder),
					folder->getFullPath () == inbox->getFullPath () ?
						FolderType::Inbox :
						ToFolderType (attrs.getSpecialUse ())
				});
		}
		emit gotFolders (folders);
	}

	QList<Message_ptr> AccountThreadWorker::FetchFullMessages (const std::vector<vmime::shared_ptr<vmime::net::message>>& messages)
	{
		const auto& context = tr ("Fetching messages for %1")
					.arg (A_->GetName ());

		auto pl = MkPgListener (context);

		QMetaObject::invokeMethod (pl,
				"start",
				Q_ARG (const int, messages.size ()));

		int i = 0;
		QList<Message_ptr> newMessages;
		Q_FOREACH (auto message, messages)
		{
			QMetaObject::invokeMethod (pl,
					"progress",
					Q_ARG (const int, ++i),
					Q_ARG (const int, messages.size ()));

			auto msgObj = FromHeaders (message);

			FullifyHeaderMessage (msgObj, message);

			newMessages << msgObj;
		}

		QMetaObject::invokeMethod (pl,
					"stop",
					Q_ARG (const int, messages.size ()));

		return newMessages;
	}

	ProgressListener* AccountThreadWorker::MkPgListener (const QString& text)
	{
		auto pl = new ProgressListener (text);
		pl->deleteLater ();
		emit gotProgressListener (ProgressListener_g_ptr (pl));
		return pl;
	}

	void AccountThreadWorker::handleMessagesChanged (const QStringList& folder, const QList<int>& numbers)
	{
		qDebug () << Q_FUNC_INFO << folder << numbers;
		auto set = vmime::net::messageSet::empty ();
		for (const auto& num : numbers)
			set.addRange (vmime::net::numberMessageRange { num });
	}

	void AccountThreadWorker::sendNoop ()
	{
		if (CachedStore_)
			CachedStore_->noop ();
	}

	void AccountThreadWorker::setNoopTimeout (int timeout)
	{
		NoopTimer_->stop ();

		if (timeout > NoopTimer_->interval ())
			sendNoop ();

		NoopTimer_->start (timeout);
	}

	void AccountThreadWorker::flushSockets ()
	{
		CachedFolders_.clear ();
		CachedStore_.reset ();
	}

	void AccountThreadWorker::synchronize (const QList<QStringList>& folders, const QByteArray& last)
	{
		const auto& store = MakeStore ();
		SyncIMAPFolders (store);
		FetchMessagesIMAP (folders, last);
	}

	void AccountThreadWorker::getMessageCount (const QStringList& folder, QObject *handler, const QByteArray& slot)
	{
		const auto& netFolder = GetFolder (folder, FolderMode::NoChange);
		if (!netFolder)
			return;

		const auto& status = netFolder->getStatus ();
		QMetaObject::invokeMethod (handler,
				slot,
				Q_ARG (int, status->getMessageCount ()),
				Q_ARG (int, status->getUnseenCount ()),
				Q_ARG (QStringList, folder));
	}

	void AccountThreadWorker::setReadStatus (bool read, const QList<QByteArray>& ids, const QStringList& folderPath)
	{
		const auto& folder = GetFolder (folderPath, FolderMode::ReadWrite);
		if (!folder)
			return;

		folder->setMessageFlags (ToMessageSet (ids),
				vmime::net::message::Flags::FLAG_SEEN,
				read ?
						vmime::net::message::FLAG_MODE_ADD :
						vmime::net::message::FLAG_MODE_REMOVE);

		QList<Message_ptr> messages;
		for (const auto& id : ids)
		{
			const auto& message = Core::Instance ().GetStorage ()->LoadMessage (A_, folderPath, id);
			message->SetRead (read);

			messages << message;
		}

		emit gotUpdatedMessages (messages, folderPath);
	}

	void AccountThreadWorker::fetchWholeMessage (Message_ptr origMsg)
	{
		if (!origMsg)
			return;

		const QByteArray& sid = origMsg->GetFolderID ();
		auto folder = GetFolder (origMsg->GetFolders ().value (0), FolderMode::ReadOnly);
		if (!folder)
			return;

		try
		{
			const auto& set = vmime::net::messageSet::byUID (sid.constData ());
			const auto attrs = vmime::net::fetchAttributes::FLAGS |
					vmime::net::fetchAttributes::UID |
					vmime::net::fetchAttributes::CONTENT_INFO |
					vmime::net::fetchAttributes::STRUCTURE |
					vmime::net::fetchAttributes::FULL_HEADER;
			const auto& messages = folder->getAndFetchMessages (set, attrs);
			if (messages.empty ())
			{
				qWarning () << Q_FUNC_INFO
						<< "message with ID"
						<< sid.toHex ()
						<< "not found in"
						<< messages.size ();
				return;
			}

			FullifyHeaderMessage (origMsg, messages.front ());
		}
		catch (const vmime::exceptions::invalid_response& resp)
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid response"
					<< resp.response ().c_str ()
					<< "to command"
					<< resp.command ().c_str ();
			return;
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "failed to fetch the message:"
					<< e.what ();
			return;
		}

		qDebug () << "done";

		emit messageBodyFetched (origMsg);
	}

	void AccountThreadWorker::fetchAttachment (Message_ptr msg,
			const QString& attName, const QString& path)
	{
		const auto& msgId = msg->GetFolderID ();

		const auto& folder = GetFolder (msg->GetFolders ().value (0), FolderMode::ReadWrite);
		const auto& msgSet = vmime::net::messageSet::byUID (msgId.constData ());
		const auto& messages = folder->getAndFetchMessages (msgSet, vmime::net::fetchAttributes::STRUCTURE);
		if (messages.empty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "message with ID"
					<< msgId.toHex ()
					<< "not found in"
					<< messages.size ();
			return;
		}

		vmime::messageParser mp (messages.front ()->getParsedMessage ());
		for (const auto& att : mp.getAttachmentList ())
		{
			if (StringizeCT (att->getName ()) != attName)
				continue;

			auto data = att->getData ();

			QFile file (path);
			if (!file.open (QIODevice::WriteOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to open"
						<< path
						<< file.errorString ();
				return;
			}

			OutputIODevAdapter adapter (&file);
			data->extract (adapter,
					MkPgListener (tr ("Fetching attachment %1...").arg (attName)));

			break;
		}
	}

	void AccountThreadWorker::copyMessages (const QList<QByteArray>& ids,
			const QStringList& from, const QList<QStringList>& tos)
	{
		if (ids.isEmpty () || tos.isEmpty ())
			return;

		const auto& folder = GetFolder (from, FolderMode::ReadWrite);
		if (!folder)
			return;

		const auto& set = ToMessageSet (ids);
		for (const auto& to : tos)
			folder->copyMessages (Folder2Path (to), set);
	}

	void AccountThreadWorker::deleteMessages (const QList<QByteArray>& ids, const QStringList& path)
	{
		if (ids.isEmpty ())
			return;

		const auto& folder = GetFolder (path, FolderMode::ReadWrite);
		if (!folder)
			return;

		folder->deleteMessages (ToMessageSet (ids));
		folder->expunge ();
		emit gotMessagesRemoved (ids, path);
	}

	namespace
	{
		vmime::shared_ptr<vmime::mailbox> FromPair (const QPair<QString, QString>& pair)
		{
			return vmime::make_shared<vmime::mailbox> (vmime::text (pair.first.toUtf8 ().constData ()),
					pair.second.toUtf8 ().constData ());
		}

		vmime::addressList ToAddressList (const Message::Addresses_t& addresses)
		{
			vmime::addressList result;
			for (const auto& pair : addresses)
				result.appendAddress (FromPair (pair));
			return result;
		}

		vmime::messageIdSequence ToMessageIdSequence (const QList<QByteArray>& ids)
		{
			vmime::messageIdSequence result;
			for (const auto& id : ids)
				result.appendMessageId (vmime::make_shared<vmime::messageId> (id.constData ()));
			return result;
		}

		vmime::messageId GenerateMsgId (const Message_ptr& msg)
		{
			const auto& senderAddress = msg->GetAddress (Message::Address::From).second;

			const auto& contents = msg->GetBody ().toUtf8 ();
			const auto& contentsHash = QCryptographicHash::hash (contents, QCryptographicHash::Sha1).toBase64 ();

			const auto& now = QDateTime::currentDateTime ();

			const auto& id = now.toString ("ddMMyyyy.hhmmsszzzap") + '%' + contentsHash + '%' + senderAddress;

			return vmime::string { id.toUtf8 ().constData () };
		}
	}

	void AccountThreadWorker::sendMessage (const Message_ptr& msg)
	{
		if (!msg)
			return;

		vmime::messageBuilder mb;
		mb.setSubject (vmime::text (msg->GetSubject ().toUtf8 ().constData ()));
		mb.setExpeditor (*FromPair (msg->GetAddress (Message::Address::From)));
		mb.setRecipients (ToAddressList (msg->GetAddresses (Message::Address::To)));
		mb.setCopyRecipients (ToAddressList (msg->GetAddresses (Message::Address::Cc)));
		mb.setBlindCopyRecipients (ToAddressList (msg->GetAddresses (Message::Address::Bcc)));

		const auto& html = msg->GetHTMLBody ();

		if (html.isEmpty ())
		{
			mb.getTextPart ()->setCharset (vmime::charsets::UTF_8);
			mb.getTextPart ()->setText (vmime::make_shared<vmime::stringContentHandler> (msg->GetBody ().toUtf8 ().constData ()));
		}
		else
		{
			mb.constructTextPart ({ vmime::mediaTypes::TEXT, vmime::mediaTypes::TEXT_HTML });
			const auto& textPart = vmime::dynamicCast<vmime::htmlTextPart> (mb.getTextPart ());
			textPart->setCharset (vmime::charsets::UTF_8);
			textPart->setText (vmime::make_shared<vmime::stringContentHandler> (html.toUtf8 ().constData ()));
			textPart->setPlainText (vmime::make_shared<vmime::stringContentHandler> (msg->GetBody ().toUtf8 ().constData ()));
		}

		for (const auto& descr : msg->GetAttachments ())
		{
			try
			{
				const QFileInfo fi (descr.GetName ());
				const auto& att = vmime::make_shared<vmime::fileAttachment> (descr.GetName ().toUtf8 ().constData (),
						vmime::mediaType (descr.GetType ().constData (), descr.GetSubType ().constData ()),
						vmime::text (descr.GetDescr ().toUtf8 ().constData ()));
				att->getFileInfo ().setFilename (fi.fileName ().toUtf8 ().constData ());
				att->getFileInfo ().setSize (descr.GetSize ());

				mb.appendAttachment (att);
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "failed to append"
						<< descr.GetName ()
						<< e.what ();
			}
		}

		const auto& vMsg = mb.construct ();
		const auto& userAgent = QString ("LeechCraft Snails %1")
				.arg (Core::Instance ().GetProxy ()->GetVersion ());
		const auto& header = vMsg->getHeader ();
		header->UserAgent ()->setValue (userAgent.toUtf8 ().constData ());

		if (!msg->GetInReplyTo ().isEmpty ())
			header->InReplyTo ()->setValue (ToMessageIdSequence (msg->GetInReplyTo ()));
		if (!msg->GetReferences ().isEmpty ())
			header->References ()->setValue (ToMessageIdSequence (msg->GetReferences ()));

		header->MessageId ()->setValue (GenerateMsgId (msg));

		auto pl = MkPgListener (tr ("Sending message %1...").arg (msg->GetSubject ()));
		auto transport = MakeTransport ();
		try
		{
			if (!transport->isConnected ())
				transport->connect ();
		}
		catch (const vmime::exceptions::authentication_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "authentication error:"
					<< e.what ()
					<< "with response"
					<< e.response ().c_str ();
			throw;
		}
		transport->send (vMsg, pl);
	}
}
}
