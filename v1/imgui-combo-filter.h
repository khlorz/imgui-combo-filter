// Improved ComboAutoSelect by @sweihub and from contributions of other people on making and improving this widget

// Changes:
//  - Templated ComboAutoSelect function instead of void*
//  - Uses fuzzy search callback so fuzzy searching algorithm can be user implemented. Different data types calls for different fuzzy search algorithm.
//  - An overload is made with a fuzzy search from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h, if you don't need/want to implement your own fuzzy search algorithm
//  - The function can work with c-style arrays and most std containers. For other containers, it should have size() method to call
//  - Some minor improvements on the function

//#define __ENABLE_COMBOAUTOSELECT_HELPER__ // Uncomment to enable use of ComboAutoSelectHelper struct

#pragma once

#include <type_traits> // For std::enable_if to minimize template error dumps
#include "imgui.h"
#include "imgui_internal.h"

//----------------------------------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

// Callback for container of your choice
// Index can be negative or out of range so you can customize the return value for invalid index
template<typename T>
using ComboItemGetterCallback = const char* (*)(T items, int index);

// ComboAutoSelect search callback
// Creating the callback can be templated (recommended) or made for a specific container type
// The callback should return the index of an item choosen by the fuzzy search algorithm. Return -1 for failure.
template<typename T>
using ComboAutoSelectSearchCallback = int (*)(T items, const char* search_string, ComboItemGetterCallback<T> getter_callback);

// Combo box with text filter
// T1 should be a container.
// T2 can be, but not necessarily, the same as T1 but it should be convertible from T1 (e.g. std::vector<...> -> std::span<...>)
// It should be noted that because T1 is a const reference, T2 should be correctly const
// Template deduction should work so no need for typing out the types when using the function (C++17 or later)
// To work with c-style arrays, you might need to use std::span<...>, or make your own wrapper if not applicable, for T2 to query for its size inside ItemGetterCallback
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelect(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboAutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags = ImGuiComboFlags_None);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelect(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ImGuiComboFlags flags = ImGuiComboFlags_None);

namespace Internal
{

// Created my own std::size and std::empty implementation to avoid additional header dependency
template<typename T>
constexpr auto GetContainerSize(const T& item);
template<typename T, unsigned long long N>
constexpr unsigned long long GetContainerSize(const T(&array)[N]) noexcept;
template<typename T>
constexpr bool IsContainerEmpty(const T& item);
template<typename T, unsigned long long N>
constexpr bool IsContainerEmpty(const T(&array)[N]) noexcept;

bool FuzzySearchEX(char const* pattern, char const* src, int& out_score);
bool FuzzySearchEX(char const* pattern, char const* haystack, int& out_score, unsigned char matches[], int maxMatches, int& outMatches);
template<typename T>
int DefaultAutoSelectSearchCallback(T items, const char* str, ComboItemGetterCallback<T> item_getter);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelectEX(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboAutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags);

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

template<typename T1, typename T2, typename>
bool ComboAutoSelect(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboAutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags)
{
	ImGui::BeginDisabled(Internal::IsContainerEmpty(items));
	bool ret = Internal::ComboAutoSelectEX(combo_label, input_text, input_capacity, selected_item, items, item_getter, autoselect_callback, flags);
	ImGui::EndDisabled();

	return ret;
}

template<typename T1, typename T2, typename>
bool ComboAutoSelect(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ImGuiComboFlags flags)
{
	return ComboAutoSelect(combo_label, input_text, input_capacity, selected_item, items, item_getter, Internal::DefaultAutoSelectSearchCallback, flags);
}

namespace Internal
{

template<typename T> 
constexpr auto GetContainerSize(const T& item)
{
    return item.size();
}

template<typename T, unsigned long long N>
constexpr unsigned long long GetContainerSize(const T(&array)[N]) noexcept
{
	return N;
}

template<typename T>
constexpr bool IsContainerEmpty(const T& item)
{
	return item.size() == 0;
}

template<typename T, unsigned long long N>
constexpr bool IsContainerEmpty(const T(&array)[N]) noexcept
{
	return false;
}

template<typename T>
int DefaultAutoSelectSearchCallback(T items, const char* str, ComboItemGetterCallback<T> item_getter)
{
	if (str[0] == '\0')
		return -1;

	const int item_count = static_cast<int>(Internal::GetContainerSize(items));
	constexpr int max_matches = 128;
	unsigned char matches[max_matches];
	int best_item = -1;
	int prevmatch_count;
	int match_count;
	int best_score;
	int score;
	int i = 0;

	for (; i < item_count; ++i) {
		if (FuzzySearchEX(str, item_getter(items, i), score, matches, max_matches, match_count)) {
			prevmatch_count = match_count;
			best_score = score;
			best_item = i;
			break;
		}
	}
	for (; i < item_count; ++i) {
		if (FuzzySearchEX(str, item_getter(items, i), score, matches, max_matches, match_count)) {
			if ((score > best_score && prevmatch_count >= match_count) || (score == best_score && match_count > prevmatch_count)) {
				prevmatch_count = match_count;
				best_score = score;
				best_item = i;
			}
		}
	}

	return best_item;
}

template<typename T1, typename T2, typename>
bool ComboAutoSelectEX(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboAutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags)
{
	// Always consume the SetNextWindowSizeConstraint() call in our early return paths
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

	const ImGuiID popupId = window->GetID(combo_label);
	const ImGuiStyle& style = g.Style;

	const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
	const ImVec2 label_size = CalcTextSize(combo_label, NULL, true);
	const float expected_w = CalcItemWidth();
	const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : expected_w;
	const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + label_size.y + style.FramePadding.y * 2.0f));
	const ImRect total_bb(frame_bb.Min, ImVec2((label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f) + frame_bb.Max.x, frame_bb.Max.y));
	const float value_x2 = ImMax(frame_bb.Min.x, frame_bb.Max.x - arrow_size);
	ItemSize(total_bb, style.FramePadding.y);
	if (!ItemAdd(total_bb, popupId, &frame_bb))
		return false;

	bool hovered, held;
	bool pressed = ButtonBehavior(frame_bb, popupId, &hovered, &held);
	bool popupIsAlreadyOpened = IsPopupOpen(popupId, 0); //ImGuiPopupFlags_AnyPopupLevel);
	bool popupJustOpened = false;

	if (!popupIsAlreadyOpened) {
		if (pressed) {
			OpenPopupEx(popupId);
			popupIsAlreadyOpened = true;
			popupJustOpened = true;
		}
		else {
			const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
			RenderNavHighlight(frame_bb, popupId);
			if (!(flags & ImGuiComboFlags_NoPreview))
				window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
		}
	}
	if (!(flags & ImGuiComboFlags_NoArrowButton)) {
		ImU32 bg_col = GetColorU32((popupIsAlreadyOpened || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImU32 text_col = GetColorU32(ImGuiCol_Text);
		window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
		if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
			RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col, popupIsAlreadyOpened ? ImGuiDir_Up : ImGuiDir_Down, 1.0f);
	}

	if (!popupIsAlreadyOpened) {
		RenderFrameBorder(frame_bb.Min, frame_bb.Max, style.FrameRounding);
		if (input_text != NULL && !(flags & ImGuiComboFlags_NoPreview)) {
			RenderTextClipped(
				ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y),
				ImVec2(value_x2, frame_bb.Max.y),
				input_text,
				NULL,
				NULL,
				ImVec2(0.0f, 0.0f)
			);
		}
	}

	if (label_size.x > 0)
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), combo_label);
	if (!popupIsAlreadyOpened)
		return false;

	const float popup_width = (flags & (ImGuiComboFlags_NoPreview | ImGuiComboFlags_NoArrowButton)) ? expected_w : w - arrow_size;
	int popup_item_count = -1;
	if (!(g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
		if ((flags & ImGuiComboFlags_HeightMask_) == 0)
			flags |= ImGuiComboFlags_HeightRegular;
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_)); // Only one
		if      (flags & ImGuiComboFlags_HeightRegular) popup_item_count = 8;
		else if (flags & ImGuiComboFlags_HeightSmall)   popup_item_count = 4;
		else if (flags & ImGuiComboFlags_HeightLarge)   popup_item_count = 20;
		const float popup_height = popup_item_count < 0 ? FLT_MAX : (g.FontSize + g.Style.ItemSpacing.y) * (popup_item_count + 1) - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2) + 5.00f; // Increment popup_item_count to account for the InputText widget
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(popup_width, popup_height));
	}

	char name[16];
	ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

	// Peek into expected window size so we can position it
	if (ImGuiWindow* popup_window = FindWindowByName(name)) {
		if (popup_window->WasActive)
		{
			// Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
			ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
			popup_window->AutoPosLastDirection = (flags & ImGuiComboFlags_PopupAlignLeft) ? ImGuiDir_Left : ImGuiDir_Down; // Left = "Below, Toward Left", Down = "Below, Toward Right (default)"
			ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
			ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, frame_bb, ImGuiPopupPositionPolicy_ComboBox);
			const float ypos_offset = flags & ImGuiComboFlags_NoPreview ? 0.0f : label_size.y + (style.FramePadding.y * 2.0f);
			if (pos.y < frame_bb.Min.y)
				pos.y += ypos_offset;
			else
				pos.y -= ypos_offset;
			if (pos.x > frame_bb.Min.x)
				pos.x += frame_bb.Max.x;

			SetNextWindowPos(pos);
		}
	}

	// Horizontally align ourselves with the framed text
	constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.50f, 5.00f));
	if (!Begin(name, NULL, window_flags)) {
		PopStyleVar();
		PopItemWidth();
		EndPopup();
		IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
		return false;
	}

	PushItemWidth(GetWindowWidth());
	SetCursorPos(ImVec2(0.f, window->DC.CurrLineTextBaseOffset));
	if (popupJustOpened) {
		SetKeyboardFocusHere(0);
	}

	bool selection_jump             = false;
	bool selection_scroll           = false;
	bool selectionChanged           = false;
	const bool clicked_outside      = !IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && IsMouseClicked(0);
	const int items_count           = static_cast<int>(Internal::GetContainerSize(items));
	const int actual_input_capacity = input_capacity - 1;

	PushStyleVar(ImGuiStyleVar_FrameRounding, 7.50f);
	PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(240, 240, 240, 255));
	PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 0, 0, 255));
	const bool buffer_changed = InputTextEx("##inputText", NULL, input_text, actual_input_capacity, ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll, NULL, NULL);
	PopStyleColor(2);
	PopStyleVar(1);
	PopItemWidth();

	if (clicked_outside || IsKeyPressed(ImGuiKey_Escape)) { // Resets the selection to it's initial value if the user exits the combo (clicking outside the combo or the combo arrow button)
		if (clicked_outside)
			strncpy(input_text, g.InputTextState.InitialTextA.Data, actual_input_capacity);
		selected_item = -1;
		for (int i = 0; i < items_count; ++i) {
			if (strcmp(input_text, item_getter(items, i)) == 0) {
				selected_item = i;
				break;
			}
		}
		if (selected_item < 0)
			ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
		else
			selection_jump = true;
		CloseCurrentPopup();
	}
	else if (buffer_changed) {
		selected_item = autoselect_callback(items, input_text, item_getter);
		if (selected_item < 0)
			SetNextWindowScroll(ImVec2(0.0f, 0.0f));
		else
			selection_jump = true;
	}
	else if (IsKeyPressed(GetKeyIndex(ImGuiKey_Enter)) || IsKeyPressed(GetKeyIndex(ImGuiKey_KeypadEnter))) { // Automatically exit the combo popup on selection
		if (strcmp(input_text, g.InputTextState.InitialTextA.Data)) {
			selectionChanged = true;
			strncpy(input_text, item_getter(items, selected_item), actual_input_capacity);
		}
		CloseCurrentPopup();
	}

	constexpr ImGuiWindowFlags listbox_flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus; //0; //ImGuiWindowFlags_HorizontalScrollbar
	if (popup_item_count > items_count || popup_item_count < 0)
		popup_item_count = items_count;
	char listbox_name[16];
	ImFormatString(listbox_name, 16, "##lbn%u", popupId); // Create different listbox name/id per combo so scroll position does not persist on every combo
	if (BeginListBox(listbox_name, ImVec2(popup_width - style.ItemSpacing.x + 2.00f, (g.FontSize + g.Style.ItemSpacing.y) * popup_item_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2) - 2.50f))) {
		g.CurrentWindow->Flags |= listbox_flags;

		if (IsKeyPressed(GetKeyIndex(ImGuiKey_UpArrow))) {
			if (selected_item > 0)
			{
				(selected_item) -= 1;
				selection_scroll = true;
			}
		}
		else if (IsKeyPressed(GetKeyIndex(ImGuiKey_DownArrow))) {
			if (selected_item >= -1 && selected_item < items_count - 1)
			{
				selected_item += 1;
				selection_scroll = true;
			}
		}

		char select_item_id[128];
		for (int n = 0; n < items_count; n++)
		{
			bool is_selected = n == selected_item;
			const char* select_value = item_getter(items, n);

			// allow empty item / in case of duplicate item name on different index
			ImFormatString(select_item_id, sizeof(select_item_id), "%s##item_%02d", select_value, n);
			if (Selectable(select_item_id, is_selected)) {
				if (!is_selected) {
					selectionChanged = true;
					selected_item = n;
					strncpy(input_text, select_value, actual_input_capacity);
				}
				CloseCurrentPopup();
			}

			if (is_selected && (IsWindowAppearing() || selection_jump || selection_scroll)) {
				SetScrollHereY();
			}
		}

		if (selection_scroll && selected_item > -1) {
			const char* sActiveidxValue2 = item_getter(items, selected_item);
			strncpy(input_text, sActiveidxValue2, actual_input_capacity);

			ImGuiInputTextState& intxt_state = g.InputTextState;
			const char* buf_end = NULL;
			intxt_state.CurLenW = ImTextStrFromUtf8(intxt_state.TextW.Data, intxt_state.TextW.Size, input_text, NULL, &buf_end);
			intxt_state.CurLenA = (int)(buf_end - input_text);
			intxt_state.CursorClamp();
		}

		EndListBox();
	}
	EndPopup();
	PopStyleVar();

	return selectionChanged;
}

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// HELPERS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{
	
#ifdef __ENABLE_COMBOAUTOSELECT_HELPER__
// My own implementation of helper for ComboAutoSelect.
// My helper is still rough and flawed, but it works for me. Feel free to comment out the #define value to disable this helper.
// As you can see from the template deduction guide below, it can accept r-values, lvalues, and pointers, but it can't make a copy of the objects, only constructed or moved.

template<typename T1, typename T2>
struct ComboAutoSelectData
{
	constexpr static int BufferSize = 128;

	char InputBuffer[BufferSize];
	int  SelectedItem;

	T1                                Items;
	ComboItemGetterCallback<T2>       ItemGetter;
	ComboAutoSelectSearchCallback<T2> ItemSearcher;

	template<typename = std::enable_if_t<std::is_convertible_v<std::conditional_t<std::is_pointer_v<T1>, std::remove_pointer_t<T1>, T1>, T2>>>
	ComboAutoSelectData(T1&& combo_items, ComboItemGetterCallback<T2> item_getter_callback, ComboAutoSelectSearchCallback<T2> fuzzy_search_callback = Internal::DefaultAutoSelectSearchCallback) :
		InputBuffer(),
		SelectedItem(-1),
		Items(std::forward<T1>(combo_items)),
		ItemGetter(item_getter_callback),
		ItemSearcher(fuzzy_search_callback)
	{
		if constexpr (std::is_pointer_v<T1>)
			strncpy(InputBuffer, ItemGetter(*Items, SelectedItem), BufferSize - 1);
		else
			strncpy(InputBuffer, ItemGetter(Items, SelectedItem), BufferSize - 1);
	}

	void Reset()
	{
		SelectedItem = -1;
		if constexpr (std::is_pointer_v<T1>)
			strncpy(InputBuffer, ItemGetter(*Items, SelectedItem), BufferSize - 1);
		else
			strncpy(InputBuffer, ItemGetter(Items, SelectedItem), BufferSize - 1);
	}
};
template<typename T1, typename T2>
ComboAutoSelectData(T1&&, ComboItemGetterCallback<T2>, ComboAutoSelectSearchCallback<T2> = Internal::DefaultAutoSelectSearchCallback) -> ComboAutoSelectData<T1, T2>;
template<typename T1, typename T2>
ComboAutoSelectData(T1&, ComboItemGetterCallback<T2>, ComboAutoSelectSearchCallback<T2> = Internal::DefaultAutoSelectSearchCallback) -> ComboAutoSelectData<const T1&, T2>;
template<typename T1, typename T2>
ComboAutoSelectData(T1*, ComboItemGetterCallback<T2>, ComboAutoSelectSearchCallback<T2> = Internal::DefaultAutoSelectSearchCallback) -> ComboAutoSelectData<const T1*, T2>;

template<typename T1, typename T2>
bool ComboAutoSelect(const char* combo_label, ComboAutoSelectData<T1, T2>& combo_data, ImGuiComboFlags flags = ImGuiComboFlags_None)
{
	if constexpr (std::is_pointer_v<T1>)
		return ComboAutoSelect(combo_label, combo_data.InputBuffer, combo_data.BufferSize, combo_data.SelectedItem, *combo_data.Items, combo_data.ItemGetter, combo_data.ItemSearcher, flags);
	else
		return ComboAutoSelect(combo_label, combo_data.InputBuffer, combo_data.BufferSize, combo_data.SelectedItem, combo_data.Items, combo_data.ItemGetter, combo_data.ItemSearcher, flags);
}
#endif // __ENABLE_COMBOAUTOSELECT_HELPER__
	
} // ImGui namespace
