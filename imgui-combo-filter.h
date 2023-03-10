// Improved ComboAutoSelect by @sweihub and from contributions of other people on making and improving this widget

// Changes:
//  - Templated ComboFilter function instead of void*
//  - Uses fuzzy search callback so fuzzy searching algorithm can be user implemented. Different data types calls for different fuzzy search algorithm.
//  - An overload is made with a fuzzy search from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h, if you don't need/want to implement your own fuzzy search algorithm
//  - The function can work with c-style arrays and most std containers. For other containers, it should have size() method to call
//  - Some minor improvements on the function

#pragma once

#include <type_traits> // For std::enable_if to minimize template error dumps
#include "imgui.h"
#include "imgui_internal.h"

// Callback for container of your choice
// Index can be negative so you can customize the return value for invalid index
template<typename T>
using ItemGetterCallback = const char* (*)(T items, int index);

// Fuzzy search callback
// Creating the callback can be templated (recommended) or made for a specific container type
// The callback should return the index of an item choosen by the fuzzy search algorithm. Return -1 for failure.
template<typename T>
using FuzzySearchCallback = int (*)(T items, const char* search_string, ItemGetterCallback<T> getter_callback);

//----------------------------------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

// Combo box with text filter
// T1 should be a container.
// T2 can be, but not necessarily, the same as T1 and convertible from T1 (std::vector<...> -> std::span<...>)
// Template deduction should work so no need for typing out the types when using the function (C++17 or later)
// To work with c-style arrays, you might need to use std::span<...>, or make your own wrapper if not applicable, to query for its size inside ItemGetterCallback
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboFilter(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, FuzzySearchCallback<T2> fuzzy_search, ImGuiComboFlags flags = ImGuiComboFlags_None);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboFilter(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, ImGuiComboFlags flags = ImGuiComboFlags_None);

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
int FuzzySearch(T items, const char* str, ItemGetterCallback<T> item_getter);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboFilterEX(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, FuzzySearchCallback<T2> fuzzy_search, ImGuiComboFlags flags);

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

template<typename T1, typename T2, typename>
bool ComboFilter(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, FuzzySearchCallback<T2> fuzzy_search, ImGuiComboFlags flags)
{
	ImGui::BeginDisabled(Internal::IsContainerEmpty(items));
	bool ret = Internal::ComboFilterEX(combo_label, input_text, input_capacity, selected_item, items, item_getter, fuzzy_search, flags);
	ImGui::EndDisabled();

	return ret;
}

template<typename T1, typename T2, typename>
bool ComboFilter(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, ImGuiComboFlags flags)
{
	return ComboFilter(combo_label, input_text, input_capacity, selected_item, items, item_getter, Internal::FuzzySearch, flags);
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
int FuzzySearch(T items, const char* str, ItemGetterCallback<T> item_getter)
{
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
			if ((score > best_score && prevmatch_count >= match_count)) {
				prevmatch_count = match_count;
				best_score = score;
				best_item = i;
			}
		}
	}

	return best_item;
}

template<typename T1, typename T2, typename>
bool ComboFilterEX(const char* combo_label, char* input_text, int input_capacity, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, FuzzySearchCallback<T2> fuzzy_search, ImGuiComboFlags flags)
{
	// Always consume the SetNextWindowSizeConstraint() call in our early return paths
	ImGuiContext& g = *GImGui;

	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	// Call the getter to obtain the preview string which is a parameter to BeginCombo()
	const int items_count = static_cast<int>(Internal::GetContainerSize(items));
	const ImGuiID popupId = window->GetID(combo_label);
	bool popupIsAlreadyOpened = IsPopupOpen(popupId, 0); //ImGuiPopupFlags_AnyPopupLevel);
	const char* sActiveidxValue1 = item_getter(items, selected_item);
	bool popupNeedsToBeOpened = (input_text[0] != 0) && (sActiveidxValue1 && strcmp(input_text, sActiveidxValue1));
	bool popupJustOpened = false;

	IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

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

	if (!popupIsAlreadyOpened) {
		const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		RenderNavHighlight(frame_bb, popupId);
		if (!(flags & ImGuiComboFlags_NoPreview))
			window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
	}
	if (!(flags & ImGuiComboFlags_NoArrowButton)) {
		ImU32 bg_col = GetColorU32((popupIsAlreadyOpened || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImU32 text_col = GetColorU32(ImGuiCol_Text);
		window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
		if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
			RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col, ImGuiDir_Down, 1.0f);
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

		if ((pressed || g.NavActivateId == popupId || popupNeedsToBeOpened) && !popupIsAlreadyOpened) {
			if (window->DC.NavLayerCurrent == 0)
				window->NavLastIds[0] = popupId;
			OpenPopupEx(popupId);
			popupIsAlreadyOpened = true;
			popupJustOpened = true;
		}
	}

	if (label_size.x > 0) {
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), combo_label);
	}

	if (!popupIsAlreadyOpened) {
		return false;
	}

	const float totalWMinusArrow = w - arrow_size;
	struct ImGuiSizeCallbackWrapper {
		static void sizeCallback(ImGuiSizeCallbackData* data) {
			float* totalWMinusArrow = (float*)(data->UserData);
			data->DesiredSize = ImVec2(*totalWMinusArrow, 200.f);
		}
	};
	SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(totalWMinusArrow, 150.f), ImGuiSizeCallbackWrapper::sizeCallback, (void*)&totalWMinusArrow);

	char name[16];
	ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

	// Peek into expected window size so we can position it
	if (ImGuiWindow* popup_window = FindWindowByName(name)) {
		if (popup_window->WasActive) {
			ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
			if (flags & ImGuiComboFlags_PopupAlignLeft)
				popup_window->AutoPosLastDirection = ImGuiDir_Left;
			ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
			ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, frame_bb, ImGuiPopupPositionPolicy_ComboBox);

			pos.y -= label_size.y + style.FramePadding.y * 2.0f;

			SetNextWindowPos(pos);
		}
	}

	// Horizontally align ourselves with the framed text
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	// PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
	bool ret = Begin(name, NULL, window_flags);

	PushItemWidth(GetWindowWidth());
	SetCursorPos(ImVec2(0.f, window->DC.CurrLineTextBaseOffset));
	if (popupJustOpened) {
		SetKeyboardFocusHere(0);
	}

	bool buffer_changed = InputTextEx("##inputText", NULL, input_text, input_capacity, ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll, NULL, NULL);
	bool done = IsItemDeactivatedAfterEdit() || (IsItemDeactivated() && !IsItemHovered());
	PopItemWidth();

	if (!ret) {
		EndChild();
		PopItemWidth();
		EndPopup();
		IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
		return false;
	}

	ImGuiWindowFlags window_flags2 = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus; //0; //ImGuiWindowFlags_HorizontalScrollbar
	BeginChild("ChildL", ImVec2(GetContentRegionAvail().x, GetContentRegionAvail().y), false, window_flags2);

	bool selectionChanged = false;
	if (buffer_changed && input_text[0] != '\0') {
		int new_idx = fuzzy_search(items, input_text, item_getter);
		int idx = new_idx >= 0 ? new_idx : selected_item;
		selectionChanged = selected_item != idx;
		selected_item = idx;
	}

	bool arrowScroll = false;
	//int arrowScrollIdx = *current_item;

	if (IsKeyPressed(GetKeyIndex(ImGuiKey_UpArrow))) {
		if (selected_item > 0)
		{
			(selected_item) -= 1;
			arrowScroll = true;
			SetWindowFocus();
		}
	}
	if (IsKeyPressed(GetKeyIndex(ImGuiKey_DownArrow))) {
		if (selected_item >= -1 && selected_item < items_count - 1)
		{
			selected_item += 1;
			arrowScroll = true;
			SetWindowFocus();
		}
	}

	// select the first match
	if (IsKeyPressed(GetKeyIndex(ImGuiKey_Enter)) || IsKeyPressed(GetKeyIndex(ImGuiKey_KeypadEnter))) {
		arrowScroll = true;
		int current_selected_item = selected_item;
		selected_item = fuzzy_search(items, input_text, item_getter);
		if (selected_item < 0) {
			strncpy(input_text, item_getter(items, current_selected_item), input_capacity - 1);
			selected_item = current_selected_item;
		}
		CloseCurrentPopup();
	}

	if (IsKeyPressed(GetKeyIndex(ImGuiKey_Backspace))) {
		selected_item = fuzzy_search(items, input_text, item_getter);
		selectionChanged = true;
	}

	if (done && !arrowScroll) {
		if (strcmp(sActiveidxValue1, input_text) != 0 || selected_item < 0) {
			strncpy(input_text, item_getter(items, -1), input_capacity - 1);
			selected_item = -1;
		}
		CloseCurrentPopup();
	}

	bool done2 = false;

	for (int n = 0; n < items_count; n++)
	{
		bool is_selected = n == selected_item;
		if (is_selected && (IsWindowAppearing() || selectionChanged)) {
			SetScrollHereY();
		}

		if (is_selected && arrowScroll) {
			SetScrollHereY();
		}

		const char* select_value = item_getter(items, n);

		// allow empty item
		char item_id[128];
		ImFormatString(item_id, sizeof(item_id), "%s##item_%02d", select_value, n);
		if (Selectable(item_id, is_selected)) {
			selectionChanged = selected_item != n;
			selected_item = n;
			strncpy(input_text, select_value, input_capacity - 1);
			CloseCurrentPopup();
			done2 = true;
		}
	}

	if (arrowScroll && selected_item > -1) {
		const char* sActiveidxValue2 = item_getter(items, selected_item);
		strncpy(input_text, sActiveidxValue2, input_capacity - 1);
		ImGuiWindow* wnd = FindWindowByName(name);
		const ImGuiID id = wnd->GetID("##inputText");
		ImGuiInputTextState* state = GetInputTextState(id);

		const char* buf_end = NULL;
		state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, input_text, NULL, &buf_end);
		state->CurLenA = (int)(buf_end - input_text);
		state->CursorClamp();
	}

	EndChild();
	EndPopup();

	const char* sActiveidxValue3 = item_getter(items, selected_item);
	bool ret1 = (selectionChanged && (sActiveidxValue3 && !strcmp(sActiveidxValue3, input_text)));

	bool widgetRet = done || done2 || ret1;

	return widgetRet;
}

} // Internal namespace
} // ImGui namespace
