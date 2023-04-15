// Improved ComboAutoSelect by @sweihub and from contributions of other people on making and improving this widget

// Changes:
//  - Templated ComboAutoSelect function instead of void*
//  - Uses fuzzy search callback so fuzzy searching algorithm can be user implemented. Different data types calls for different fuzzy search algorithm.
//  - An overload is made with a fuzzy search from https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h, if you don't need/want to implement your own fuzzy search algorithm
//  - The function can work with c-style arrays and most std containers. For other containers, it should have size() method to call
//  - Some minor improvements on the function

#pragma once

#include <type_traits> // For std::enable_if to minimize template error dumps
#include "imgui.h"
#include "imgui_internal.h"

// Callback for container of your choice
// Index can be negative or out of range so you can customize the return value for invalid index
template<typename T>
using ItemGetterCallback = const char* (*)(T items, int index);

// ComboAutoSelect search callback
// Creating the callback can be templated (recommended) or made for a specific container type
// The callback should return the index of an item choosen by the fuzzy search algorithm. Return -1 for failure.
template<typename T>
using AutoSelectSearchCallback = int (*)(T items, const char* search_string, ItemGetterCallback<T> getter_callback);

//----------------------------------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

struct ComboAutoSelectData;

void ClearComboData(const char* window_name, const char* combo_name);
void ClearComboData(const char* combo_name);
void ClearComboData(ImGuiID combo_id);

// Combo box with text filter
// T1 should be a container.
// T2 can be, but not necessarily, the same as T1 but it should be convertible from T1 (e.g. std::vector<...> -> std::span<...>)
// It should be noted that because T1 is a const reference, T2 should be correctly const
// Template deduction should work so no need for typing out the types when using the function (C++17 or later)
// To work with c-style arrays, you might need to use std::span<...>, or make your own wrapper if not applicable, for T2 to query for its size inside ItemGetterCallback
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelect(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, AutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags = ImGuiComboFlags_None);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelect(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, ImGuiComboFlags flags = ImGuiComboFlags_None);

namespace Internal
{

struct ComboData;

template<class T>
T* AddComboData(const char* window_name, const char* combo_name);
template<class T>
T* AddComboData(const char* combo_name);
template<class T>
T* AddComboData(ImGuiID combo_id);
template<class T>
T* GetComboData(const char* window_name, const char* combo_name);
template<class T>
T* GetComboData(const char* combo_name);
template<class T>
T* GetComboData(ImGuiID combo_id);

float CalcComboItemHeight(int item_count, float offset_multiplier = 1.0f);

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
int DefaultAutoSelectSearchCallback(T items, const char* str, ItemGetterCallback<T> item_getter);
template<typename T1, typename T2, typename = std::enable_if<std::is_convertible<T1, T2>::value>::type>
bool ComboAutoSelectEX(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, AutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags);

} // Internal namespace
} // ImGui namespace

//----------------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------------

namespace ImGui
{

namespace Internal
{

struct ComboData
{
	static constexpr int StringCapacity = 128;
	char InputText[StringCapacity + 1]{ 0 };
	struct
	{
		const char* Preview;
		int         Index;
	} InitialValues{ "", -1 };
	int CurrentSelection{ -1 };

	virtual ~ComboData() = default;
};

}

struct ComboAutoSelectData : Internal::ComboData
{
	bool SetNewValue(const char* new_val, int new_index) noexcept;
	bool SetNewValue(const char* new_val) noexcept;
	void ResetToInitialValue() noexcept;
	void Reset() noexcept;
};

template<typename T1, typename T2, typename>
bool ComboAutoSelect(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, AutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags)
{
	ImGui::BeginDisabled(Internal::IsContainerEmpty(items));
	bool ret = Internal::ComboAutoSelectEX(combo_label, selected_item, items, item_getter, autoselect_callback, flags);
	ImGui::EndDisabled();

	return ret;
}

template<typename T1, typename T2, typename>
bool ComboAutoSelect(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, ImGuiComboFlags flags)
{
	return ComboAutoSelect(combo_label, selected_item, items, item_getter, Internal::DefaultAutoSelectSearchCallback, flags);
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
int DefaultAutoSelectSearchCallback(T items, const char* str, ItemGetterCallback<T> item_getter)
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
bool ComboAutoSelectEX(const char* combo_label, int& selected_item, const T1& items, ItemGetterCallback<T2> item_getter, AutoSelectSearchCallback<T2> autoselect_callback, ImGuiComboFlags flags)
{
	// Always consume the SetNextWindowSizeConstraint() call in our early return paths
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = GetCurrentWindow();

	ImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.Flags;
	g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
	if (window->SkipItems)
		return false;

	IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

	const ImGuiStyle& style = g.Style;
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

	ComboAutoSelectData* combo_data = GetComboData<ComboAutoSelectData>(combo_id);
	if (!combo_data) {
		combo_data = AddComboData<ComboAutoSelectData>(combo_id);
	}

	// Open on click
	bool hovered, held;
	bool pressed = ButtonBehavior(bb, combo_id, &hovered, &held);
	bool popupIsAlreadyOpened = IsPopupOpen(combo_id, ImGuiPopupFlags_None);
	bool popupJustOpened = false;

	const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
	if (!popupIsAlreadyOpened) {
		if (pressed) {
			OpenPopupEx(combo_id);
			popupIsAlreadyOpened = true;
			popupJustOpened = true;
		}
		const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		RenderNavHighlight(bb, combo_id);
		if (!(flags & ImGuiComboFlags_NoPreview))
			window->DrawList->AddRectFilled(bb.Min, ImVec2(value_x2, bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
	}
	if (!(flags & ImGuiComboFlags_NoArrowButton)) {
		ImU32 bg_col = GetColorU32((popupIsAlreadyOpened || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImU32 text_col = GetColorU32(ImGuiCol_Text);
		window->DrawList->AddRectFilled(ImVec2(value_x2, bb.Min.y), bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight);
		if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
			RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y), text_col, popupIsAlreadyOpened ? ImGuiDir_Up : ImGuiDir_Down, 1.0f);
	}

	if (!popupIsAlreadyOpened) {
		RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);
		if (combo_data->InitialValues.Preview != NULL && !(flags & ImGuiComboFlags_NoPreview)) {
			RenderTextClipped(
				ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y),
				ImVec2(value_x2, bb.Max.y),
				combo_data->InitialValues.Preview,
				NULL,
				NULL,
				ImVec2(0.0f, 0.0f)
			);
		}
	}

	if (label_size.x > 0)
		RenderText(ImVec2(bb.Max.x + style.ItemInnerSpacing.x, bb.Min.y + style.FramePadding.y), combo_label);
	if (!popupIsAlreadyOpened)
		return false;

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.50f, 5.00f));
	const float popup_width = flags & (ImGuiComboFlags_NoPreview | ImGuiComboFlags_NoArrowButton) ? expected_w : w - arrow_size;
	int popup_item_count = -1;
	if (!(g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
		if ((flags & ImGuiComboFlags_HeightMask_) == 0)
			flags |= ImGuiComboFlags_HeightRegular;
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_)); // Only one
		if (flags & ImGuiComboFlags_HeightRegular)    popup_item_count = 8 + 1;
		else if (flags & ImGuiComboFlags_HeightSmall) popup_item_count = 4 + 1;
		else if (flags & ImGuiComboFlags_HeightLarge) popup_item_count = 20 + 1;
		SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(popup_width, CalcComboItemHeight(popup_item_count, 4.00f)));
	}

	char name[16];
	ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

	// Peek into expected window size so we can position it
	if (ImGuiWindow* popup_window = FindWindowByName(name)) {
		if (popup_window->WasActive) {
			// Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
			ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
			popup_window->AutoPosLastDirection = (flags & ImGuiComboFlags_PopupAlignLeft) ? ImGuiDir_Left : ImGuiDir_Down; // Left = "Below, Toward Left", Down = "Below, Toward Right (default)"
			ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
			ImVec2 pos = FindBestWindowPosForPopupEx(bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, bb, ImGuiPopupPositionPolicy_ComboBox);
			const float ypos_offset = flags & ImGuiComboFlags_NoPreview ? 0.0f : label_size.y + (style.FramePadding.y * 2.0f);
			if (pos.y < bb.Min.y)
				pos.y += ypos_offset;
			else
				pos.y -= ypos_offset;
			if (pos.x > bb.Min.x)
				pos.x += bb.Max.x;

			SetNextWindowPos(pos);
		}
	}

	// Horizontally align ourselves with the framed text
	constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
	if (!Begin(name, NULL, window_flags)) {
		PopStyleVar();
		EndPopup();
		IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
		return false;
	}

	if (popupJustOpened) {
		SetKeyboardFocusHere(0);
	}

	const float items_max_width = popup_width - style.WindowPadding.x * 2.0f;
	SetCursorPos(ImVec2(style.WindowPadding.x, window->DC.CurrLineTextBaseOffset));
	PushItemWidth(items_max_width);
	PushStyleVar(ImGuiStyleVar_FrameRounding, 2.50f);
	PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(240, 240, 240, 255));
	PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 0, 0, 255));
	const bool buffer_changed = InputTextEx("##inputText", NULL, combo_data->InputText, combo_data->StringCapacity, ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll, NULL, NULL);
	PopStyleColor(2);
	PopStyleVar(1);
	PopItemWidth();

	const bool clicked_outside = !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AnyWindow) && IsMouseClicked(0);
	bool selection_scroll = false;
	bool selection_jump = false;
	bool selection_changed = false;

	if (clicked_outside || IsKeyPressed(ImGuiKey_Escape)) { // Resets the selection to it's initial value if the user exits the combo (clicking outside the combo or the combo arrow button)
		if (combo_data->CurrentSelection != combo_data->InitialValues.Index)
			combo_data->ResetToInitialValue();
		if (combo_data->InitialValues.Index < 0)
			SetNextWindowScroll(ImVec2(0.0f, 0.0f));
		else
			selection_jump = true;
		CloseCurrentPopup();
	}
	else if (buffer_changed) {
		combo_data->CurrentSelection = autoselect_callback(items, combo_data->InputText, item_getter);
		if (combo_data->CurrentSelection < 0)
			SetNextWindowScroll(ImVec2(0.0f, 0.0f));
		else
			selection_jump = true;
	}
	else if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) { // Automatically exit the combo popup on selection
		if (combo_data->SetNewValue(item_getter(items, combo_data->CurrentSelection))) {
			selection_jump = true;
			selection_changed = true;
			selected_item = combo_data->CurrentSelection;
		}
		CloseCurrentPopup();
	}

	char listbox_name[16];
	ImFormatString(listbox_name, 16, "##lbn%u", combo_id); // Create different listbox name/id per combo so scroll position does not persist on every combo
	const int items_count = static_cast<int>(GetContainerSize(items));
	if (--popup_item_count > items_count || popup_item_count < 0)
		popup_item_count = items_count;
	if (BeginListBox(listbox_name, ImVec2(items_max_width, CalcComboItemHeight(popup_item_count, 1.25f)))) {
		g.CurrentWindow->Flags |= ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;

		if (IsKeyPressed(ImGuiKey_UpArrow)) {
			if (combo_data->CurrentSelection > 0)
			{
				combo_data->CurrentSelection -= 1;
				selection_scroll = true;
			}
		}
		else if (IsKeyPressed(ImGuiKey_DownArrow)) {
			if (combo_data->CurrentSelection >= -1 && combo_data->CurrentSelection < items_count - 1)
			{
				combo_data->CurrentSelection += 1;
				selection_scroll = true;
			}
		}

		char select_item_id[128];
		for (int n = 0; n < items_count; n++)
		{
			bool is_selected = n == combo_data->CurrentSelection;
			const char* select_value = item_getter(items, n);

			// allow empty item / in case of duplicate item name on different index
			ImFormatString(select_item_id, sizeof(select_item_id), "%s##item_%02d", select_value, n);
			if (Selectable(select_item_id, is_selected)) {
				if (combo_data->SetNewValue(select_value, n)) {
					selection_jump = true;
					selection_changed = true;
					selected_item = combo_data->CurrentSelection;
				}
				CloseCurrentPopup();
			}

			if (is_selected && (IsWindowAppearing() || selection_jump || selection_scroll)) {
				SetScrollHereY();
			}
		}

		if (selection_scroll && combo_data->CurrentSelection > -1) {
			const char* sActiveidxValue2 = item_getter(items, combo_data->CurrentSelection);
			strncpy(combo_data->InputText, sActiveidxValue2, combo_data->StringCapacity);

			ImGuiInputTextState& intxt_state = g.InputTextState;
			const char* buf_end = NULL;
			intxt_state.CurLenW = ImTextStrFromUtf8(intxt_state.TextW.Data, intxt_state.TextW.Size, combo_data->InputText, NULL, &buf_end);
			intxt_state.CurLenA = (int)(buf_end - combo_data->InputText);
			intxt_state.CursorClamp();
		}

		EndListBox();
	}
	EndPopup();
	PopStyleVar();

	return selection_changed;
}

} // Internal namespace
} // ImGui namespace
