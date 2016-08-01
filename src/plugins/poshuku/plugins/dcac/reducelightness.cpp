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

#include "reducelightness.h"
#include <QImage>
#include <util/sys/cpufeatures.h>
#include "effectscommon.h"

#ifdef SSE_ENABLED
#include "ssecommon.h"
#endif

namespace LeechCraft
{
namespace Poshuku
{
namespace DCAC
{
	namespace
	{
		void ReduceLightnessInner (unsigned char* pixel, float recipFactor)
		{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
			pixel [0] *= recipFactor;
			pixel [1] *= recipFactor;
			pixel [2] *= recipFactor;
#else
			pixel [1] *= recipFactor;
			pixel [2] *= recipFactor;
			pixel [3] *= recipFactor;
#endif
		}

		void ReduceLightnessDefault (QImage& image, float factor)
		{
			const auto height = image.height ();
			const auto width = image.width ();

			for (int y = 0; y < height; ++y)
			{
				const auto scanline = image.scanLine (y);
				for (int x = 0; x < width; ++x)
					ReduceLightnessInner (&scanline [x * 4], 1 / factor);
			}
		}

#ifdef SSE_ENABLED
		uint32_t BSRL (uint32_t a)
		{
			uint32_t res;
			__asm ("bsrl %1, %0" : "=r"(res) : "r"(a) : );
			return res;
		}

		struct Divide
		{
			__m128i M_;
			__m128i S1_;
			__m128i S2_;

			Divide (uint16_t factor)
			{
				const uint16_t log = BSRL (factor - 1) + 1;
				const uint16_t twoToLog = 1 << log;

				M_ = _mm_set1_epi16 (1 + static_cast<uint16_t> ((static_cast<uint32_t> (twoToLog - factor) << 16) / factor));
				S1_ = _mm_setr_epi32 (1, 0, 0, 0);
				S2_ = _mm_setr_epi32 (log - 1, 0, 0, 0);
			}
		};

		__attribute__ ((target ("ssse3")))
		void ReduceLightnessSSSE3 (QImage& image, float factor)
		{
			constexpr auto alignment = 16;

			const auto height = image.height ();
			const auto width = image.width ();

			const __m128i pixel1msk = MakeMask<128, 7, 0, 1> ();
			const __m128i pixel2msk = MakeMask<128, 15, 8, 1> ();

			const __m128i pixel1revmask = MakeRevMask<128, 8, 0> ();
			const __m128i pixel2revmask = MakeRevMask<128, 8, 1> ();

			const __m128i orMask = _mm_set1_epi32 (0xff000000);

			const Divide div { static_cast<uint16_t> (std::round (256 * factor)) };

			factor = 1 / factor;

			for (int y = 0; y < height; ++y)
			{
				uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;
				auto handler = [scanline, factor] (int i) { ReduceLightnessInner (&scanline [i], factor); };
				HandleLoopBegin<alignment> (scanline, width, x, bytesCount, handler);

				for (; x < bytesCount; x += alignment)
				{
					__m128i fourPixels = _mm_load_si128 (reinterpret_cast<const __m128i*> (scanline + x));

					__m128i pair1 = _mm_shuffle_epi8 (fourPixels, pixel1msk);
					__m128i p1s1 = _mm_mulhi_epu16 (pair1, div.M_);
					__m128i p1s2 = _mm_sub_epi16 (pair1, p1s1);
					pair1 = _mm_srl_epi16 (p1s2, div.S1_);
					pair1 = _mm_add_epi16 (p1s1, pair1);
					pair1 = _mm_srl_epi16 (pair1, div.S2_);
					pair1 = _mm_shuffle_epi8 (pair1, pixel1revmask);

					__m128i pair2 = _mm_shuffle_epi8 (fourPixels, pixel2msk);
					__m128i p2s1 = _mm_mulhi_epu16 (pair2, div.M_);
					__m128i p2s2 = _mm_sub_epi16 (pair2, p2s1);
					pair2 = _mm_srl_epi16 (p2s2, div.S1_);
					pair2 = _mm_add_epi16 (p2s1, pair2);
					pair2 = _mm_srl_epi16 (pair2, div.S2_);
					pair2 = _mm_shuffle_epi8 (pair2, pixel2revmask);

					fourPixels = _mm_or_si128 (pair1, pair2);
					fourPixels = _mm_or_si128 (fourPixels, orMask);

					_mm_store_si128 (reinterpret_cast<__m128i*> (scanline + x), fourPixels);
				}

				HandleLoopEnd (width, x, handler);
			}
		}

		__attribute__ ((target ("avx2")))
		void ReduceLightnessAVX2 (QImage& image, float factor)
		{
			constexpr auto alignment = 32;

			const auto height = image.height ();
			const auto width = image.width ();

			const __m256i pixel1msk = MakeMask<256, 7, 0, 1> ();
			const __m256i pixel2msk = MakeMask<256, 15, 8, 1> ();

			const __m256i pixel1revmask = MakeRevMask<256, 8, 0> ();
			const __m256i pixel2revmask = MakeRevMask<256, 8, 1> ();

			const __m256i orMask = _mm256_set1_epi32 (0xff000000);

			const Divide div { static_cast<uint16_t> (std::round (256 * factor)) };
			const __m256i mult = _mm256_broadcastq_epi64 (div.M_);

			factor = 1 / factor;

			for (int y = 0; y < height; ++y)
			{
				uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;
				auto handler = [scanline, factor] (int i) { ReduceLightnessInner (&scanline [i], factor); };
				HandleLoopBegin<alignment> (scanline, width, x, bytesCount, handler);

				for (; x < bytesCount; x += alignment)
				{
					__m256i eightPixels = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x));

					__m256i p1 = _mm256_shuffle_epi8 (eightPixels, pixel1msk);
					{
						__m256i multh1 = _mm256_mulhi_epu16 (p1, mult);
						p1 = _mm256_sub_epi16 (p1, multh1);
						p1 = _mm256_srl_epi16 (p1, div.S1_);
						p1 = _mm256_add_epi16 (multh1, p1);
						p1 = _mm256_srl_epi16 (p1, div.S2_);
					}
					p1 = _mm256_shuffle_epi8 (p1, pixel1revmask);

					__m256i p2 = _mm256_shuffle_epi8 (eightPixels, pixel2msk);
					{
						__m256i multh2 = _mm256_mulhi_epu16 (p2, mult);
						p2 = _mm256_sub_epi16 (p2, multh2);
						p2 = _mm256_srl_epi16 (p2, div.S1_);
						p2 = _mm256_add_epi16 (multh2, p2);
						p2 = _mm256_srl_epi16 (p2, div.S2_);
					}
					p2 = _mm256_shuffle_epi8 (p2, pixel2revmask);

					eightPixels = _mm256_or_si256 (p1, p2);
					eightPixels = _mm256_or_si256 (eightPixels, orMask);

					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x), eightPixels);
				}

				HandleLoopEnd (width, x, handler);
			}
		}
	}
#endif

	void ReduceLightness (QImage& image, float factor)
	{
		if (std::abs (factor - 1) < 1e-3)
			return;

#ifdef SSE_ENABLED
		static const auto ptr = Util::CpuFeatures::Choose ({
					{ Util::CpuFeatures::Feature::AVX2, &ReduceLightnessAVX2 },
					{ Util::CpuFeatures::Feature::SSSE3, &ReduceLightnessSSSE3 }
				},
				&ReduceLightnessDefault);

		ptr (image, factor);
#else
		ReduceLightnessDefault (image, factor);
#endif
	}
}
}
}
