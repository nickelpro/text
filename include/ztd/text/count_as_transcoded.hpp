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

#pragma once

#ifndef ZTD_TEXT_count_as_transcoded_HPP
#define ZTD_TEXT_count_as_transcoded_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_point.hpp>
#include <ztd/text/default_encoding.hpp>
#include <ztd/text/count_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/detail/is_lossless.hpp>
#include <ztd/text/detail/transcode_routines.hpp>
#include <ztd/text/detail/encoding_range.hpp>
#include <ztd/text/detail/validate_count_routines.hpp>

#include <ztd/idk/span.hpp>
#include <ztd/idk/type_traits.hpp>
#include <ztd/idk/char_traits.hpp>
#include <ztd/idk/tag.hpp>

#include <string_view>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_count_as_transcoded ztd::text::count_as_transcoded
	///
	/// @brief These functions use a variety of means to count the number of code units that will result from the
	/// input code units after a transcoding operation.
	///
	/// @{

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when an intermediary decoding operation fails.
	/// @param[in] __to_error_handler The error handler to invoke when the final encoding operation fails.
	/// @param[in,out] __from_state The state attached to the `__from_encoding` that will be used for the intermediary
	/// decode step.
	/// @param[in,out] __to_state The state related to the `__to_encoding` that will be used for the final encoding
	/// step.
	/// @param[in, out] __pivot A reference to a descriptor of a (potentially usable) pivot range, usually a range of
	/// contiguous data from a span provided by the implementation but customizable by the end-user. If the
	/// intermediate conversion is what failed, then the ztd::text::pivot's `error_code` member will be set to that
	/// error. This only happens if the overall operation also fails, and need not be checked unless to obtain
	/// additional information for when a top-level operation fails.
	///
	/// @returns A ztd::text::count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t) and a reference
	/// to the provided `__from_state` and `__to_state` .
	///
	/// @remarks This method will not check any ADL extension points. A combination of implementation techniques will
	/// be used to count code units, with a loop over the `.encode_one` / `.decode_one` call into an intermediate,
	/// unseen buffer being the most basic choice.
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler,
		typename _ToErrorHandler, typename _FromState, typename _ToState, typename _PivotRange>
	constexpr auto basic_count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding,
		_ToEncoding&& __to_encoding, _FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler,
		_FromState& __from_state, _ToState& __to_state, pivot<_PivotRange>& __pivot) {
		using _UInput         = remove_cvref_t<_Input>;
		using _InputValueType = ranges::range_value_type_t<_UInput>;
		using _WorkingInput   = ranges::range_reconstruct_t<::std::conditional_t<::std::is_array_v<_UInput>,
               ::std::conditional_t<is_char_traitable_v<_InputValueType>, ::std::basic_string_view<_InputValueType>,
                    ::ztd::span<const _InputValueType>>,
               _UInput>>;
		using _UFromEncoding  = remove_cvref_t<_FromEncoding>;
		using _UToEncoding    = remove_cvref_t<_ToEncoding>;
		using _Result         = count_transcode_result<_WorkingInput, _FromState, _ToState>;

		_WorkingInput __working_input(
			ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::forward<_Input>(__input)));

		::std::size_t __code_unit_count = 0;
		::std::size_t __errors_handled  = 0;

#define ZTD_TEXT_BASIC_COUNT_AS_TRANSCODED_LOOP_BODY_CORE_I_()                                                   \
	if (__result.error_code != encoding_error::ok) {                                                            \
		return _Result(::std::move(__result.input), __code_unit_count, __result.from_state, __result.to_state, \
		     __result.error_code, __errors_handled);                                                           \
	}                                                                                                           \
	__code_unit_count += __result.count;                                                                        \
	__errors_handled += __result.handled_errors;                                                                \
	__working_input = ::std::move(__result.input);                                                              \
	if (ranges::ranges_adl::adl_empty(__working_input)) {                                                       \
		if (!text::is_state_complete(__result.from_state)) {                                                   \
			continue;                                                                                         \
		}                                                                                                      \
		if (!text::is_state_complete(__result.to_state)) {                                                     \
			continue;                                                                                         \
		}                                                                                                      \
		break;                                                                                                 \
	}                                                                                                           \
	do {                                                                                                        \
	} while (0)

#define ZTD_TEXT_BASIC_COUNT_AS_TRANSCODED_LOOP_BODY_I_(...)                                                        \
	auto __result                                                                                                  \
	     = __VA_ARGS__(::ztd::tag<_UFromEncoding, _UToEncoding> {}, ::std::move(__working_input), __from_encoding, \
	          __to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state, __pivot);         \
	ZTD_TEXT_BASIC_COUNT_AS_TRANSCODED_LOOP_BODY_CORE_I_()

		if constexpr (is_detected_v<__txt_detail::__detect_adl_text_count_as_transcoded_one, _WorkingInput,
			              _FromEncoding, _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState, _ToState,
			              _PivotRange>) {
			for (;;) {
				ZTD_TEXT_BASIC_COUNT_AS_TRANSCODED_LOOP_BODY_I_(text_count_as_transcoded_one);
			}
		}
		else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_count_as_transcoded_one,
			                   _WorkingInput, _FromEncoding, _ToEncoding, _FromErrorHandler, _ToErrorHandler,
			                   _FromState, _ToState, _PivotRange>) {
			for (;;) {
				ZTD_TEXT_BASIC_COUNT_AS_TRANSCODED_LOOP_BODY_I_(__text_count_as_transcoded_one);
			}
		}
		else {
			using _CodeUnit = code_unit_t<_UToEncoding>;

			_CodeUnit __output_storage[max_code_units_v<_UToEncoding>] {};
			::ztd::span<_CodeUnit, max_code_units_v<_UToEncoding>> __output(__output_storage);

			for (;;) {
				auto __result = transcode_one_into(::std::move(__working_input), __from_encoding, __output,
					__to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state, __pivot);
				if (__result.error_code != encoding_error::ok) {
					return _Result(::std::move(__result.input), __code_unit_count, __result.from_state,
						__result.to_state, __result.error_code, __errors_handled);
				}
				::std::size_t __written = static_cast<::std::size_t>(__result.output.data() - __output.data());
				__code_unit_count += __written;
				__errors_handled += __result.handled_errors;
				__working_input = ::std::move(__result.input);
				if (ranges::ranges_adl::adl_empty(__working_input)) {
					if (!text::is_state_complete(__from_state)) {
						continue;
					}
					if (!text::is_state_complete(__to_state)) {
						continue;
					}
					break;
				}
			}
		}
		return _Result(::std::move(__working_input), __code_unit_count, __from_state, __to_state, encoding_error::ok,
			__errors_handled);
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when an intermediary decoding operation fails.
	/// @param[in] __to_error_handler The error handler to invoke when the final encoding operation fails.
	/// @param[in,out] __from_state The state attached to the `__from_encoding` that will be used for the intermediary
	/// decode step.
	/// @param[in,out] __to_state The state related to the `__to_encoding` that will be used for the final encoding
	/// step.
	/// @param[in, out] __pivot A reference to a descriptor of a (potentially usable) pivot range, usually a range of
	/// contiguous data from a span provided by the implementation but customizable by the end-user. If the
	/// intermediate conversion is what failed, then the ztd::text::pivot's `error_code` member will be set to that
	/// error. This only happens if the overall operation also fails, and need not be checked unless to obtain
	/// additional information for when a top-level operation fails.
	///
	/// @returns A ztd::text::count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t) and a reference
	/// to the provided `__from_state` and `__to_state` .
	///
	/// @remarks This method checks for the ADL extension point `text_count_as_transcoded` . It will be called if it
	/// is possible. Otherwise, this function will defer to ztd::text::basic_count_as_transcoded.
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler,
		typename _ToErrorHandler, typename _FromState, typename _ToState, typename _PivotRange>
	constexpr auto count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler, _FromState& __from_state,
		_ToState& __to_state, pivot<_PivotRange>& __pivot) {
		if constexpr (is_detected_v<__txt_detail::__detect_adl_text_count_as_transcoded, _Input, _FromEncoding,
			              _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState, _ToState>) {
			return text_count_as_transcoded(
				::ztd::tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
				::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
				::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
				::std::forward<_ToErrorHandler>(__to_error_handler), __from_state, __to_state, __pivot);
		}
		else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_count_as_transcoded, _Input,
			                   _FromEncoding, _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState,
			                   _ToState>) {
			return __text_count_as_transcoded(
				::ztd::tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
				::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
				::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
				::std::forward<_ToErrorHandler>(__to_error_handler), __from_state, __to_state, __pivot);
		}
		else {
			return basic_count_as_transcoded(::std::forward<_Input>(__input),
				::std::forward<_FromEncoding>(__from_encoding), ::std::forward<_ToEncoding>(__to_encoding),
				::std::forward<_FromErrorHandler>(__from_error_handler),
				::std::forward<_ToErrorHandler>(__to_error_handler), __from_state, __to_state, __pivot);
		}
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when an intermediary decoding operation fails.
	/// @param[in] __to_error_handler The error handler to invoke when the final encoding operation fails.
	/// @param[in,out] __from_state The state attached to the `__from_encoding` that will be used for the intermediary
	/// decode step.
	/// @param[in,out] __to_state The state attached to the `__to_encoding` that will be used for the final encode
	/// step.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks This method will call ztd::text::count_as_transcoded(input, from_encoding, to_encoding,
	/// from_error_handler, to_error_handler, from_state, to_state) with an `to_state` created by
	/// ztd::text::make_encode_state(to_encoding).
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler,
		typename _ToErrorHandler, typename _FromState, typename _ToState>
	constexpr auto count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler, _FromState& __from_state,
		_ToState& __to_state) {
		using _UFromEncoding = remove_cvref_t<_FromEncoding>;
		using _CodePoint     = code_point_t<_UFromEncoding>;
		using _PivotRange    = ::ztd::span<_CodePoint, max_code_points_v<_UFromEncoding>>;
		_CodePoint __intermediate_storage[max_code_points_v<_UFromEncoding>] {};
		pivot<_PivotRange> __pivot { _PivotRange(__intermediate_storage), encoding_error::ok };
		return count_as_transcoded(::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
			::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
			::std::forward<_ToErrorHandler>(__to_error_handler), __from_state, __to_state, __pivot);
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when an intermediary decoding operation fails.
	/// @param[in] __to_error_handler The error handler to invoke when the final encoding operation fails.
	/// @param[in,out] __from_state The state attached to the `__from_encoding` that will be used for the intermediary
	/// decode step.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks This method will call ztd::text::count_as_transcoded(input, from_encoding, to_encoding,
	/// from_error_handler, to_error_handler, from_state, to_state) with an `to_state` created by
	/// ztd::text::make_encode_state(to_encoding).
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler,
		typename _ToErrorHandler, typename _FromState>
	constexpr auto count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler, _FromState& __from_state) {
		using _UToEncoding = remove_cvref_t<_ToEncoding>;
		using _State       = encode_state_t<_UToEncoding>;

		_State __to_state = make_encode_state(__to_encoding);

		auto __result
			= count_as_transcoded(::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
			     ::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
			     ::std::forward<_ToErrorHandler>(__to_error_handler), __from_state, __to_state);
		return __txt_detail::__slice_to_stateless(::std::move(__result));
	}


	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when the decode portion of the transcode operation
	/// fails.
	/// @param[in] __to_error_handler The error handler to invoke when the encode portion of the transcode operation
	/// fails.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks This method will call ztd::text::count_as_transcoded(input, from_encoding, to_encoding,
	/// from_error_handler, to_error_handler, from_state) with an `from_state` created by
	/// ztd::text::make_decode_state(from_encoding).
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler,
		typename _ToErrorHandler>
	constexpr auto count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler) {
		using _UFromEncoding = remove_cvref_t<_FromEncoding>;
		using _State         = decode_state_t<_UFromEncoding>;

		_State __from_state = make_decode_state(__from_encoding);

		return count_as_transcoded(::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
			::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
			::std::forward<_ToErrorHandler>(__to_error_handler), __from_state);
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	/// @param[in] __from_error_handler The error handler to invoke when the decode portion of the transcode operation
	/// fails.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks This method will call ztd::text::count_as_transcoded(input, from_encoding, to_encoding,
	/// from_error_handler, to_error_handler) by creating an `to_error_handler` similar to
	/// ztd::text::default_handler_t.
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler>
	constexpr auto count_as_transcoded(_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler) {
		auto __handler = __txt_detail::__duplicate_or_be_careless(__from_error_handler);

		return count_as_transcoded(::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
			::std::forward<_ToEncoding>(__to_encoding), ::std::forward<_FromErrorHandler>(__from_error_handler),
			__handler);
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __from_encoding The encoding that is going to be used to decode the input into an intermediary
	/// output.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the intermediary output.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks This method will call ztd::text::count_as_transcoded(Input, Encoding, ErrorHandler) by creating
	/// an `error_handler` similar to ztd::text::default_handler_t.
	template <typename _Input, typename _FromEncoding, typename _ToEncoding>
	constexpr auto count_as_transcoded(
		_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding) {
		default_handler_t __from_error_handler {};
		return count_as_transcoded(::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
			::std::forward<_ToEncoding>(__to_encoding), __from_error_handler);
	}

	//////
	/// @brief Counts the number of code units that will result from attempting an transcode operation on the input
	/// code points.
	///
	/// @param[in] __input The input range (of code units) to find out how many code units of the transcoded output
	/// there are.
	/// @param[in] __to_encoding The encoding that is going to be used to encode the input into an intermediary
	/// output.
	///
	/// @returns A ztd::text::stateless_count_result that includes information about how many code units are present,
	/// taking into account any invoked errors (like replacement from ztd::text::replacement_handler_t).
	///
	/// @remarks Calls ztd::text::count_as_transcoded(Input, Encoding) with an `encoding` that is derived from
	/// ztd::text::default_code_unit_encoding.
	template <typename _Input, typename _ToEncoding>
	constexpr auto count_as_transcoded(_Input&& __input, _ToEncoding&& __to_encoding) {
		using _UInput   = remove_cvref_t<_Input>;
		using _CodeUnit = remove_cvref_t<ranges::range_value_type_t<_UInput>>;
#if ZTD_IS_ON(ZTD_STD_LIBRARY_IS_CONSTANT_EVALUATED)
		if (::std::is_constant_evaluated()) {
			// Use literal encoding instead, if we meet the right criteria
			using _FromEncoding = default_consteval_code_unit_encoding_t<_CodeUnit>;
			_FromEncoding __from_encoding {};
			return count_as_transcoded(
				::std::forward<_Input>(__input), __from_encoding, ::std::forward<_ToEncoding>(__to_encoding));
		}
		else
#endif
		{
			using _FromEncoding = default_code_unit_encoding_t<_CodeUnit>;
			_FromEncoding __from_encoding {};
			return count_as_transcoded(
				::std::forward<_Input>(__input), __from_encoding, ::std::forward<_ToEncoding>(__to_encoding));
		}
	}

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_count_as_transcoded_HPP
