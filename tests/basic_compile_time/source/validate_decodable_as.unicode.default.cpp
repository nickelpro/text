// =============================================================================
//
// ztd.text
// Copyright © 2022 JeanHeyd "ThePhD" Meneide and Shepherd's Oasis, LLC
// Contact: opensource@soasis.org
//
// Commercial License Usage
// Licensees holding valid commercial ztd.text licenses may use this file in
// accordance with the commercial license agreement provided with the
// Software or, alternatively, in accordance with the terms contained in
// a written agreement between you and Shepherd's Oasis, LLC.
// For licensing terms and conditions see your agreement. For
// further information contact opensource@soasis.org.
//
// Apache License Version 2 Usage
// Alternatively, this file may be used under the terms of Apache License
// Version 2.0 (the "License") for non-commercial use; you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// ============================================================================ //

#include <ztd/text/tests/basic_unicode_strings.hpp>

#include <ztd/text/validate_decodable_as.hpp>
#include <ztd/text/encoding.hpp>
#include <ztd/text/is_unicode_encoding.hpp>

inline namespace ztd_text_tests_basic_compile_time_validate_decodable_as_unicode_default {

	template <typename T>
	static void delayed() {
		// Larger unicode sequences, using defaults
#if ZTD_IS_ON(ZTD_STD_LIBRARY_IS_CONSTANT_EVALUATED)
		// Unicode sequences, using defaults
		// Can only be done if we have constant evaluation inside to swap to the literal_t encoding
		if constexpr (ztd::always_true_v<T> && ztd::text::is_unicode_encoding_v<ztd::text::literal_t>) {
			static_assert(ztd::text::validate_decodable_as(ztd::tests::unicode_sequence_truth_native_endian));
		}
		if constexpr (ztd::always_true_v<T> && ztd::text::is_unicode_encoding_v<ztd::text::wide_literal_t>) {
			static_assert(ztd::text::validate_decodable_as(ztd::tests::w_unicode_sequence_truth_native_endian));
		}
#endif
	}

	void instantiate() {
		delayed<void>();
	}

} // namespace ztd_text_tests_basic_compile_time_validate_decodable_as_unicode_default
