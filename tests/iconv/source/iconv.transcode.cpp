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

#include <ztd/text/encoding.hpp>
#include <ztd/text/transcode.hpp>

#include <ztd/text/tests/basic_unicode_strings.hpp>

#include <catch2/catch_all.hpp>

inline namespace ztd_text_tests_iconv_transcode {
	template <typename Encoding, typename Input>
	void check_roundtrip(Encoding& encoding, Input& input) {
		const auto& expected = input;
		auto result0         = ztd::text::transcode(
		             input, encoding, encoding, ztd::text::replacement_handler, ztd::text::replacement_handler);
		REQUIRE(result0 == expected);
	}
} // namespace ztd_text_tests_iconv_transcode

TEST_CASE("text/transcode/iconv", "iconv transcode can roundtrip") {
	SECTION("execution") {
		ztd::text::__impl::__execution_iconv encoding {};
		check_roundtrip(encoding, ztd::tests::basic_source_character_set);

		if (ztd::text::contains_unicode_encoding(encoding)) {
			check_roundtrip(encoding, ztd::tests::unicode_sequence_truth_native_endian);
		}
	}
	SECTION("wide_execution") {
		ztd::text::__impl::__wide_execution_iconv encoding {};
		check_roundtrip(encoding, ztd::tests::w_basic_source_character_set);

		if (ztd::text::contains_unicode_encoding(encoding)) {
			check_roundtrip(encoding, ztd::tests::w_unicode_sequence_truth_native_endian);
		}
	}
}
