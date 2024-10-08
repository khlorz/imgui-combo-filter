// Improved ComboAutoSelect and ComboFilter from contributions of people in https://github.com/ocornut/imgui/issues/1658

// Changes:
//  - Templated ComboAutoSelect function instead of void*
//  - Incompatible container and getter callback can be found in compile time
//  - A callback for string search can be user-defined/user-implemented but a default algorithm is provided using fuzzy search algorithm from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h
//  - An overload is made using the default string search algorithm if you don't need/want to implement your own fuzzy search algorithm
//  - Improved scrolling interaction
//  - Improved widget interaction
//  - Improved widget algorithm
//  - Now uses ImGuiListClipper so long lists will not impact performance
//  - C++17 standard
// 
// V1 CombAutoSelect/ComboFilter
//  - V1, as opposed to v2, has quite a lenghty API
//  - However, the combo data is more in your control compared to v2
//  - A helper class is provided to simplify the widget API

#pragma once

#include <type_traits> // For std::enable_if to minimize template error dumps
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"

//----------------------------------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

struct ComboFilterResultData;

template<typename T>
struct ComboAutoSelectSearchCallbackData;
template<typename T>
struct ComboFilterSearchCallbackData;

using ComboFilterSearchResults = std::vector<ComboFilterResultData>;

// Callback for container of your choice
// Index can be negative or out of range so you can customize the return value for invalid index
template<typename T>
using ComboItemGetterCallback = const char* (*)(T items, int index);

// ComboAutoSelect search callback
// Creating the callback can be templated (recommended) or made for a specific container type
// The callback should return the index of an item choosen by the fuzzy search algorithm. Return -1 for failure.
template<typename T>
using ComboAutoSelectSearchCallback = int (*)(const ComboAutoSelectSearchCallbackData<T>& callback_data);

// ComboFilter search callback
// The callback for filtering out a list of items depending on the input string
// The output 'out_items' will always start as empty everytime the function is called
// The template type should have the same type as the template type of ItemGetterCallback
template<typename T>
using ComboFilterSearchCallback = void (*)(const ComboFilterSearchCallbackData<T>& callback_data);

void SortFilterResultsDescending(ComboFilterSearchResults& filtered_items);
void SortFilterResultsAscending(ComboFilterSearchResults& filtered_items);

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

float CalcComboItemHeight(int item_count, float offset_multiplier = 1.0f);
void SetScrollToComboItemJump(ImGuiWindow* listbox_window, int index);
void SetScrollToComboItemUp(ImGuiWindow* listbox_window, int index);
void SetScrollToComboItemDown(ImGuiWindow* listbox_window, int index);
void UpdateInputTextAndCursor(char* buf, int buf_capacity, const char* new_str);

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
int DefaultAutoSelectSearchCallback(const ComboAutoSelectSearchCallbackData<T>& callback_data);
template<typename T>
void DefaultComboFilterSearchCallback(const ComboFilterSearchCallbackData<T>& callback_data);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelectEX(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboAutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboFilterEX(const char* combo_label, char* input_text, int input_capacity, int& preview_item, int& selected_item, ComboFilterSearchResults& filtered_items, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboFilterSearchCallback<T2> filter_callback, ImGuiComboFlags flags);

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

template<typename T>
struct ComboAutoSelectSearchCallbackData
{
	T                          Items;         // Read-only
	const char*                SearchString;  // Read-only
	ComboItemGetterCallback<T> ItemGetter;    // Read-only
};

template<typename T>
struct ComboFilterSearchCallbackData
{
	T                          Items;         // Read-only
	const char*                SearchString;  // Read-only
	ComboItemGetterCallback<T> ItemGetter;    // Read-only
	ComboFilterSearchResults*  FilterResults; // Output
};

struct ComboFilterResultData
{
	int Index;
	int Score;

	constexpr bool operator < (const ComboFilterResultData& other) const noexcept
	{
		return this->Score < other.Score;
	}
};

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
int DefaultAutoSelectSearchCallback(const ComboAutoSelectSearchCallbackData<T>& cbd)
{
	if (cbd.SearchString[0] == '\0')
		return -1;

	const int item_count = static_cast<int>(Internal::GetContainerSize(cbd.Items));
	constexpr int max_matches = 128;
	unsigned char matches[max_matches];
	int best_item = -1;
	int prevmatch_count;
	int match_count;
	int best_score;
	int score;
	int i = 0;

	for (; i < item_count; ++i) {
		if (FuzzySearchEX(cbd.SearchString, cbd.ItemGetter(cbd.Items, i), score, matches, max_matches, match_count)) {
			prevmatch_count = match_count;
			best_score = score;
			best_item = i;
			break;
		}
	}
	for (; i < item_count; ++i) {
		if (FuzzySearchEX(cbd.SearchString, cbd.ItemGetter(cbd.Items, i), score, matches, max_matches, match_count)) {
			if ((score > best_score && prevmatch_count >= match_count) || (score == best_score && match_count > prevmatch_count)) {
				prevmatch_count = match_count;
				best_score = score;
				best_item = i;
			}
		}
	}

	return best_item;
}

template<typename T>
void DefaultComboFilterSearchCallback(const ComboFilterSearchCallbackData<T>& callback_data)
{
	const int item_count = static_cast<int>(GetContainerSize(callback_data.Items));
	constexpr int max_matches = 128;
	unsigned char matches[max_matches];
	int best_item = -1;
	int match_count;
	int score = 0;

	for (int i = 0; i < item_count; ++i) {
		if (FuzzySearchEX(callback_data.SearchString, callback_data.ItemGetter(callback_data.Items, i), score, matches, max_matches, match_count)) {
			callback_data.FilterResults->emplace_back(i, score);
		}
	}

	SortFilterResultsDescending(*callback_data.FilterResults);
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

	constexpr ImGuiWindowFlags listbox_flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus; //0; //ImGuiWindowFlags_HorizontalScrollbar
	if (popup_item_count > items_count || popup_item_count < 0)
		popup_item_count = items_count;
	char listbox_name[16];
	ImFormatString(listbox_name, 16, "##lbn%u", popupId); // Create different listbox name/id per combo so scroll position does not persist on every combo
	if (BeginListBox(listbox_name, ImVec2(popup_width - style.ItemSpacing.x + 2.00f, (g.FontSize + g.Style.ItemSpacing.y) * popup_item_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2) - 2.50f))) {
		ImGuiWindow* listbox_window = ImGui::GetCurrentWindow();
		listbox_window->Flags |= listbox_flags;

		ImGuiListClipper list_clipper;
		list_clipper.Begin(items_count);
		char select_item_id[128];
		while (list_clipper.Step()) {
			for (int n = list_clipper.DisplayStart; n < list_clipper.DisplayEnd; n++) {
				bool is_selected = n == selected_item;
				const char* select_value = item_getter(items, n);

				// allow empty item / in case of duplicate item name on different index
				ImFormatString(select_item_id, sizeof(select_item_id), "%s##item_%02d", select_value, n);
				if (Selectable(select_item_id, is_selected)) {
					if (!is_selected) {
						selectionChanged = true;
						selected_item = n;
						strncpy(input_text, select_value, actual_input_capacity);
						SetScrollToComboItemJump(listbox_window, selected_item);
					}
					CloseCurrentPopup();
				}
			}
		}

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
				SetScrollY(0.0f);
			else
				SetScrollToComboItemJump(listbox_window, selected_item);
			CloseCurrentPopup();
		}
		else if (buffer_changed) {
			selected_item = autoselect_callback({ items, input_text, item_getter });
			if (selected_item < 0)
				SetScrollY(0.0f);
			else
				SetScrollToComboItemJump(listbox_window, selected_item);
		}
		else if (IsKeyPressed(GetKeyIndex(ImGuiKey_Enter)) || IsKeyPressed(GetKeyIndex(ImGuiKey_KeypadEnter))) { // Automatically exit the combo popup on selection
			if (strcmp(input_text, g.InputTextState.InitialTextA.Data)) {
				selectionChanged = true;
				strncpy(input_text, item_getter(items, selected_item), actual_input_capacity);
				SetScrollToComboItemJump(listbox_window, selected_item);
			}
			CloseCurrentPopup();
		}

		if (IsKeyPressed(GetKeyIndex(ImGuiKey_UpArrow))) {
			if (selected_item > 0)
			{
				SetScrollToComboItemUp(listbox_window, --selected_item);
				UpdateInputTextAndCursor(input_text, actual_input_capacity, item_getter(items, selected_item));
			}
		}
		else if (IsKeyPressed(GetKeyIndex(ImGuiKey_DownArrow))) {
			if (selected_item >= -1 && selected_item < items_count - 1)
			{
				SetScrollToComboItemDown(listbox_window, ++selected_item);
				UpdateInputTextAndCursor(input_text, actual_input_capacity, item_getter(items, selected_item));
			}
		}

		EndListBox();
	}
	EndPopup();
	PopStyleVar();

	return selectionChanged;
}

template<typename T1, typename T2, typename>
bool ComboFilterEX(const char* combo_label, char* input_text, int input_capacity, int& preview_item, int& selected_item, std::vector<ComboFilterResultData>& filtered_items, const T1& items, ComboItemGetterCallback<T2> item_getter, ComboFilterSearchCallback<T2> filter_callback, ImGuiComboFlags flags)
{
	ImGuiContext* g = GImGui;
	ImGuiWindow* window = GetCurrentWindow();

	if (window->SkipItems)
		return false;

	IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

	const ImGuiStyle& style = g->Style;
	const ImGuiID combo_id = window->GetID(combo_label);

	const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
	const ImVec2 label_size = CalcTextSize(combo_label, NULL, true);
	const float expected_w = CalcItemWidth();
	const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : CalcItemWidth();
	const ImVec2 bb_max(window->DC.CursorPos.x + w, window->DC.CursorPos.y + (label_size.y + style.FramePadding.y * 2.0f));
	const ImRect bb(window->DC.CursorPos, bb_max);
	const ImVec2 total_bb_max(bb.Max.x + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), bb.Max.y);
	const ImRect total_bb(bb.Min, total_bb_max);
	ItemSize(total_bb, style.FramePadding.y);
	if (!ItemAdd(total_bb, combo_id, &bb))
		return false;

	// Open on click
	bool hovered, held;
	bool pressed = ButtonBehavior(bb, combo_id, &hovered, &held);
	bool popup_open = IsPopupOpen(combo_id, ImGuiPopupFlags_None);
	bool popup_just_opened = false;
	if (pressed && !popup_open)
	{
		OpenPopupEx(combo_id, ImGuiPopupFlags_None);
		popup_open = true;
		popup_just_opened = true;
	}

	// Render shape
	const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
	RenderNavHighlight(bb, combo_id);
	if (!(flags & ImGuiComboFlags_NoPreview))
		window->DrawList->AddRectFilled(bb.Min, ImVec2(value_x2, bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
	if (!(flags & ImGuiComboFlags_NoArrowButton))
	{
		ImU32 bg_col = GetColorU32((popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImU32 text_col = GetColorU32(ImGuiCol_Text);
		window->DrawList->AddRectFilled(ImVec2(value_x2, bb.Min.y), bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
		if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
			RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y), text_col, popup_open ? ImGuiDir_Up : ImGuiDir_Down, 1.0f);
	}
	RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

	const bool is_filtering    = input_text[0] != '\0';
	auto item_getter2          = [&](int index) { return item_getter(items, is_filtering ? filtered_items[index].Index : index); };
	const char* preview_string = item_getter(items, preview_item);

	if (preview_string != NULL && !(flags & ImGuiComboFlags_NoPreview)) {
		const ImVec2 min_pos(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
		RenderTextClipped(min_pos, ImVec2(value_x2, bb.Max.y), preview_string, NULL, NULL);
	}
	if (label_size.x > 0)
		RenderText(ImVec2(bb.Max.x + style.ItemInnerSpacing.x, bb.Min.y + style.FramePadding.y), combo_label);
	if (!popup_open)
		return false;

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.50f, 5.00f));
	int popup_item_count = -1;
	if (!(g->NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
		if ((flags & ImGuiComboFlags_HeightMask_) == 0)
			flags |= ImGuiComboFlags_HeightRegular;
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_)); // Only one
		if (flags & ImGuiComboFlags_HeightRegular) popup_item_count = 8 + 1;
		else if (flags & ImGuiComboFlags_HeightSmall) popup_item_count = 4 + 1;
		else if (flags & ImGuiComboFlags_HeightLarge) popup_item_count = 20 + 1;
		const float popup_height = CalcComboItemHeight(popup_item_count, 5.0f); // Increment popup_item_count to account for the InputText widget
		SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(expected_w, popup_height));
	}

	char name[16];
	ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g->BeginPopupStack.Size); // Recycle windows based on depth

	if (ImGuiWindow* popup_window = FindWindowByName(name)) {
		if (popup_window->WasActive)
		{
			// Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
			ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
			popup_window->AutoPosLastDirection = (flags & ImGuiComboFlags_PopupAlignLeft) ? ImGuiDir_Left : ImGuiDir_Down; // Left = "Below, Toward Left", Down = "Below, Toward Right (default)"
			ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
			ImVec2 pos = FindBestWindowPosForPopupEx(bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, bb, ImGuiPopupPositionPolicy_ComboBox);
			SetNextWindowPos(pos);
		}
	}

	constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
	if (!Begin(name, NULL, window_flags)) {
		PopStyleVar();
		EndPopup();
		IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
		return false;
	}

	if (popup_just_opened) {
		SetKeyboardFocusHere();
	}

	bool selection_changed          = false;
	const bool clicked_outside      = !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AnyWindow) && IsMouseClicked(0);
	const float items_max_width     = expected_w - (style.WindowPadding.x * 2.00f);
	const int actual_input_capacity = input_capacity - 1;

	PushItemWidth(items_max_width);
	PushStyleVar(ImGuiStyleVar_FrameRounding, 5.00f);
	PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(240, 240, 240, 255));
	PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 0, 0, 255));
	const bool buffer_changed = InputTextEx("##inputText", NULL, input_text, actual_input_capacity, ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll, NULL, NULL);
	PopStyleColor(2);
	PopStyleVar(1);
	PopItemWidth();

	const int item_count = static_cast<int>(is_filtering ? GetContainerSize(filtered_items) : GetContainerSize(items));
	char listbox_name[16];
	ImFormatString(listbox_name, 16, "##lbn%u", combo_id);
	if (--popup_item_count > item_count || popup_item_count < 0)
		popup_item_count = item_count;
	if (BeginListBox(listbox_name, ImVec2(items_max_width, CalcComboItemHeight(popup_item_count, 1.50f)))) {
		ImGuiWindow* listbox_window = ImGui::GetCurrentWindow();
		listbox_window->Flags |= ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;

		if (listbox_window->Appearing)
			SetScrollToComboItemJump(listbox_window, preview_item);

		ImGuiListClipper listclipper;
		listclipper.Begin(item_count);
		char select_item_id[128];
		while (listclipper.Step()) {
			for (int i = listclipper.DisplayStart; i < listclipper.DisplayEnd; ++i) {
				bool is_selected = i == selected_item;
				const char* select_value = item_getter2(i);

				ImFormatString(select_item_id, 128, "%s##id%d", select_value, i);
				if (Selectable(select_item_id, is_selected)) {
					if (!is_filtering) {
						selected_item = i;
					}
					else {
						selected_item = filtered_items[i].Index;
						filtered_items.clear();
						input_text[0] = '\0';
					}
					if (selected_item != preview_item) {
						preview_item = selected_item;
						selection_changed = true;
					}
					CloseCurrentPopup();
				}
			}
		}

		if (clicked_outside || IsKeyPressed(ImGuiKey_Escape)) {
			selected_item = preview_item;
			input_text[0] = '\0';
			filtered_items.clear();
			CloseCurrentPopup();
		}
		else if (buffer_changed) {
			filtered_items.clear();
			if (input_text[0] != '\0')
				filter_callback({ items, input_text, item_getter, &filtered_items });
			selected_item = GetContainerSize(filtered_items) != 0 ? 0 : -1;
			SetScrollY(0.0f);
		}
		else if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) { // Automatically exit the combo popup on selection
			if (is_filtering) {
				if (selected_item >= 0)
					selected_item = filtered_items[selected_item].Index;
				filtered_items.clear();
				input_text[0] = '\0';
			}
			if (selected_item != preview_item) {
				preview_item = selected_item;
				selection_changed = true;
			}
			CloseCurrentPopup();
		}

		if (IsKeyPressed(ImGuiKey_UpArrow)) {
			if (selected_item > 0) {
				SetScrollToComboItemUp(listbox_window, --selected_item);
			}
		}
		else if (IsKeyPressed(ImGuiKey_DownArrow)) {
			if (selected_item >= -1 && selected_item < item_count - 1) {
				SetScrollToComboItemDown(listbox_window, ++selected_item);
			}
		}

		EndListBox();
	}
	EndPopup();
	PopStyleVar();

	return selection_changed;
}

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// HELPERS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

namespace Internal
{

template<typename T>
struct is_stl_container
{
	using test_type = std::remove_const_t<T>;

	template<typename A>
	static constexpr bool test(
		A* pt,
		A const* cpt					= nullptr,
		decltype(pt->begin())*			= nullptr,
		decltype(pt->end())*			= nullptr,
		decltype(cpt->begin())*			= nullptr,
		decltype(cpt->end())*			= nullptr,
		typename A::iterator* pi		= nullptr,
		typename A::const_iterator* pci	= nullptr,
		typename A::value_type* pv		= nullptr)
	{
		using iterator			= A::iterator;
		using const_iterator	= A::const_iterator;
		using value_type		= A::value_type;

		return  std::is_same_v<decltype(pt->begin()), iterator>			&&
				std::is_same_v<decltype(pt->end()), iterator>			&&
				std::is_same_v<decltype(cpt->begin()), const_iterator>	&&
				std::is_same_v<decltype(cpt->end()), const_iterator>	&&
				std::is_same_v<decltype(**pi), value_type&>				&&
				std::is_same_v<decltype(**pci), value_type const&>;
	}

	template<typename A>
	static constexpr bool test(...) {
		return false;
	}

	static const bool value = test<test_type>(nullptr);

};

template<typename T>
static constexpr bool is_stl_container_v = is_stl_container<T>::value;
template<typename T>
using ComboDataTypeDeduction = std::conditional_t<std::is_pointer_v<T>, T, T&>;
template<typename T>
using stl_element_t = std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;

template<typename T1, typename T2>
struct ComboData
{
	using ContainerType = std::remove_pointer_t<std::remove_reference_t<T1>>;
	using ContainerItemType = std::conditional_t<std::is_array_v<ContainerType>,
												 Internal::stl_element_t<ContainerType>,
												 std::conditional_t<Internal::is_stl_container_v<ContainerType>,
																	Internal::stl_element_t<ContainerType>,
																	void
																   >
												>;
	using GetterReturnType = std::conditional_t<std::is_pointer_v<ContainerItemType>,
												ContainerItemType,
												ContainerItemType&
											   >;

	constexpr static int BufferSize = 128;

	char _InputBuffer[BufferSize];
	int  _SelectedItem;

	T1                                _Items;
	ComboItemGetterCallback<T2>       _ItemGetter;

	template<typename = std::enable_if_t<std::is_convertible_v<ContainerType, T2>>>
	ComboData(T1 combo_items, ComboItemGetterCallback<T2> item_getter_callback) :
		_InputBuffer(),
		_SelectedItem(-1),
		_Items(std::forward<T1>(combo_items)),
		_ItemGetter(item_getter_callback)
	{

	}

	virtual constexpr void Reset(int) noexcept = 0;

	constexpr const ContainerType& GetAllItems() const noexcept
	{
		if constexpr (std::is_pointer_v<T1>)
			return *_Items;
		else
			return _Items;;
	}

	constexpr ContainerType& GetAllItems() noexcept
	{
		if constexpr (std::is_pointer_v<T1>)
			return *_Items;
		else
			return _Items;
	}

	constexpr int GetSelectedItemIndex() const noexcept
	{
		return _SelectedItem;
	}

	constexpr GetterReturnType GetSelectedItem() noexcept
	{
		return this->GetAllItems()[_SelectedItem];
	}

	constexpr const GetterReturnType GetSelectedItem() const noexcept
	{
		return this->GetAllItems()[_SelectedItem];
	}

};

}
	
// My own implementation of helper for ComboAutoSelect.
// My helper is still rough and flawed, but it works for me. Feel free to comment out the #define value to disable this helper.
// As you can see from the template deduction guide below, it can accept r-values, lvalues, and pointers, but it can't make a copy of the objects, only constructed or moved.

template<typename T1, typename T2>
struct ComboAutoSelectData : public Internal::ComboData<T1,T2>
{
	ComboAutoSelectSearchCallback<T2> _ItemSearcher;

	ComboAutoSelectData(T1&& combo_items, ComboItemGetterCallback<T2> item_getter_callback, ComboAutoSelectSearchCallback<T2> autoselect_search_callback = Internal::DefaultAutoSelectSearchCallback) :
		Internal::ComboData<T1, T2>(combo_items, item_getter_callback),
		_ItemSearcher(autoselect_search_callback)
	{
		strncpy(this->_InputBuffer, this->_ItemGetter(this->GetAllItems(), this->_SelectedItem), Internal::ComboData<T1, T2>::BufferSize - 1);
	}

	constexpr void Reset(int reset_selection = -1) noexcept
	{
		this->_SelectedItem = reset_selection;
		strncpy(this->_InputBuffer, this->_ItemGetter(this->GetAllItems(), this->_SelectedItem), Internal::ComboData<T1, T2>::BufferSize - 1);
	}

};
template<typename T1, typename T2>
ComboAutoSelectData(T1&&, ComboItemGetterCallback<T2>, ComboAutoSelectSearchCallback<T2> = Internal::DefaultAutoSelectSearchCallback) -> ComboAutoSelectData<T1, T2>;
template<typename T1, typename T2>
ComboAutoSelectData(Internal::ComboDataTypeDeduction<T1>, ComboItemGetterCallback<T2>, ComboAutoSelectSearchCallback<T2> = Internal::DefaultAutoSelectSearchCallback) -> ComboAutoSelectData<T1&, T2>;

template<typename T1, typename T2>
bool ComboAutoSelect(const char* combo_label, ComboAutoSelectData<T1, T2>& combo_data, ImGuiComboFlags flags = ImGuiComboFlags_None)
{
	return ComboAutoSelect(combo_label, combo_data._InputBuffer, combo_data.BufferSize, combo_data._SelectedItem, combo_data.GetAllItems(), combo_data._ItemGetter, combo_data._ItemSearcher, flags);
}

template<typename T1, typename T2>
struct ComboFilterData : public Internal::ComboData<T1,T2>
{
	int  _PreviewItem;

	ComboFilterSearchResults      _FilteredItems;
	ComboFilterSearchCallback<T2> _ItemSearcher;

	ComboFilterData(T1&& combo_items, ComboItemGetterCallback<T2> item_getter_callback, ComboFilterSearchCallback<T2> filter_search_callback = Internal::DefaultComboFilterSearchCallback) :
		Internal::ComboData<T1, T2>(combo_items, item_getter_callback),
		_PreviewItem(-1),
		_ItemSearcher(filter_search_callback)
	{
		_FilteredItems.reserve(Internal::GetContainerSize(this->GetAllItems()) / 2);
	}

	constexpr void Reset(int reset_selection = -1) noexcept
	{
		this->_InputBuffer[0] = '\0';
		_FilteredItems.clear();
		_PreviewItem = this->_SelectedItem = reset_selection;
	}

};
template<typename T1, typename T2>
ComboFilterData(T1&&, ComboItemGetterCallback<T2>, ComboFilterSearchCallback<T2> = Internal::DefaultComboFilterSearchCallback) -> ComboFilterData<T1, T2>;
template<typename T1, typename T2>
ComboFilterData(Internal::ComboDataTypeDeduction<T1>, ComboItemGetterCallback<T2>, ComboFilterSearchCallback<T2> = Internal::DefaultComboFilterSearchCallback) -> ComboFilterData<T1&, T2>;

template<typename T1, typename T2>
bool ComboFilter(const char* combo_label, ComboFilterData<T1, T2>& combo_data, ImGuiComboFlags flags = ImGuiComboFlags_None)
{
	BeginDisabled(Internal::IsContainerEmpty(combo_data.GetAllItems()));
	auto ret = Internal::ComboFilterEX(combo_label, combo_data._InputBuffer, combo_data.BufferSize, combo_data._PreviewItem, combo_data._SelectedItem, combo_data._FilteredItems, combo_data.GetAllItems(), combo_data._ItemGetter, combo_data._ItemSearcher, flags);
	EndDisabled();

	return ret;
}
	
} // ImGui namespace
