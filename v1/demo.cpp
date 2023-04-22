#include "demo.h"
#include "imgui-combo-filter.h"

#include <vector>
#include <array>
#include <span>
#include <string>

struct DemoPair
{
    const char* str;
    int num;
};

static const char* item_getter1(const std::vector<std::string>& items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].c_str();
    }
    return "N/A";
}

static const char* item_getter2(std::span<const std::string> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].c_str();
    }
    return "...";
}

static const char* item_getter3(std::span<const DemoPair> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].str;
    }
    return "";
}

static const char* item_getter4(std::span<const char* const> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index];
    }
    return "";
}

// Fuzzy search algorith by @r-lyeh with some adjustments of my own
// Template fuzzy search function so it can accept any container
template<typename T>
int fuzzy_search(T items, const char* str, ImGui::ItemGetterCallback<T> item_getter) {
    auto fuzzy_score = [](const char* str1, const char* str2, int& score) -> bool {
        score = 0;
        if (*str2 == '\0')
            return *str1 == '\0';

        int consecutive = 0;
        int maxerrors = 0;

        while (*str1 && *str2) {
            int is_leading = (*str1 & 64) && !(str1[1] & 64);
            if ((*str1 & ~32) == (*str2 & ~32)) {
                int had_separator = (str1[-1] <= 32);
                int x = had_separator || is_leading ? 10 : consecutive * 5;
                consecutive = 1;
                score += x;
                ++str2;
            }
            else {
                int x = -1, y = is_leading * -3;
                consecutive = 0;
                score += x;
                maxerrors += y;
            }
            ++str1;
        }

        score += (maxerrors < -9 ? -9 : maxerrors);
        return *str2 == '\0';
    };

    int items_count = static_cast<int>(std::size(items));
    int best = -1;
    int i = 0;
    int score;
    int scoremax;
    for (; i < items_count; ++i) {
        const char* word_i = item_getter(items, i);
        if (fuzzy_score(word_i, str, score)) {
            scoremax = score;
            best = i;
            break;
        }
    }
    for (; i < items_count; ++i) {
        const char* word_i = item_getter(items, i);
        if (fuzzy_score(word_i, str, score)) {
            if (score > scoremax) {
                scoremax = score;
                best = i;
            }
        }
    }
    return best;
};

void ComboAutoSelectDemo()
{
    if (ImGui::Begin("ComboAutoSelect Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::vector<std::string> items1{ "instruction", "Chemistry", "Beating Around the Bush", "Instantaneous Combustion", "Level 999999", "nasal problems", "On cloud nine", "break the iceberg", "lacircificane" };
        static char inputbuf1[128];
        static int selected_item1 = -1;
        if (ImGui::ComboAutoSelect("std::vector combo", inputbuf1, 128, selected_item1, items1, item_getter1, ImGuiComboFlags_HeightSmall)) {
            /* Selection made */
        }

        static std::array<std::string, 11> items2{ "arm86", "chicanery", "A quick brown fox", "jumps over the lazy dog", "Budaphest Hotel", "Grand", "The", "1998204", "1-0-1-0-0-1xx", "end", "Alphabet" };
        static char inputbuf2[128];
        static int selected_item2 = -1;
        if (ImGui::ComboAutoSelect("std::array combo", inputbuf2, 128, selected_item2, items2, item_getter2, fuzzy_search, ImGuiComboFlags_HeightLarge)) {
            /* Selection made */
        }

        static DemoPair items3[]{ {"bury the dark", 1}, {"bury the", 8273}, {"bury", 0}, {"bur", 777}, {"dig", 943}, {"dig your", 20553}, {"dig your motivation", 6174}, {"max concentration", 5897}, {"crazyyyy", 31811} };
        static char inputbuf3[128];
        static int selected_item3 = -1;
        if (ImGui::ComboAutoSelect("c-array combo", inputbuf3, 128, selected_item3, items3, item_getter3)) {
            /* Selection made */
        }

        static std::vector<std::string> items4;
        static char inputbuf4[128];
        static int selected_item4 = -1;
        if (ImGui::ComboAutoSelect("empty combo", inputbuf4, 128, selected_item4, items4, item_getter2, fuzzy_search)) {
            /* Selection made */
        }

#ifdef __ENABLE_COMBOAUTOSELECT_HELPER__
        // To copy a c-style array, you should find a way to wrap it up. In this case, we use std::to_array
        static const char* items5[]{ "tolerationism", "tediferous", "tach", "thermokinematics", "interamnian", "pistolography", "discerptible", "subulate", "sententious", "salt" };
        static ImGui::ComboAutoSelectData combo_data2(std::to_array(items5), item_getter4);
        if (ImGui::ComboAutoSelect("Combo 5", combo_data2)) {
            /* Selection made */
        }

        // One way to "move" a c-style array
        static std::string items6[]{ "level", "leveler", "MacroCallback.cpp", "Miskatonic university", "MockAI.h", "MockGameplayTasks.h", "MovieSceneColorTrack.cpp", "r.maxfps", "r.maxsteadyfps", "reboot", "rescale", "reset", "resource", "restart", "retrocomputer", "retrograd", "return", "slomo 10" };
        static ImGui::ComboAutoSelectData combo_data1(std::to_array(std::move(items6)), item_getter2, fuzzy_search);
        if (ImGui::ComboAutoSelect("Combo 6", combo_data1, ImGuiComboFlags_HeightLarge)) {
            /* Selection made */
        }

        // Construct the data in ComboAutoSelectData
        static ImGui::ComboAutoSelectData combo_data3(std::array<std::string, 5>{"array element 0", "Accelerando", "Soprano", "Crescendo", "Arpeggio"}, item_getter2);
        if (ImGui::ComboAutoSelect("Combo 7", combo_data3, ImGuiComboFlags_NoArrowButton)) {
            /* Selection made */
        }

        // Move the data into ComboAutoSelectData
        std::vector<std::string> items7{"exaggerate", "error", "impress", "mechanism", "Electromagnetic Impulse", "uninterestingly amazing", "Sweet and salty flavor", "zebra stripes", "strike", "sweat", "axe", "OK"};
        static ImGui::ComboAutoSelectData combo_data4(std::move(items7), item_getter1, fuzzy_search);
        if (ImGui::ComboAutoSelect("Combo 8", combo_data4, ImGuiComboFlags_HeightSmall)) {
            /* Selection made */
        }

        // Use an existing data.
        static std::vector<const char*> items8{ "rhinology", "aerolithology", "arachnology", "oncology", "morphology", "haplology", "algology", "phytology", "tetralogy", "logic" };
        static ImGui::ComboAutoSelectData combo_data5(items8, item_getter4); // By reference
        if (ImGui::ComboAutoSelect("Combo 9", combo_data5)) {
            /* Selection made */
        }
		// In this case, passing the c-style array by pointer rather than reference is the only way because it would read it as a pointer rather than a reference to the array.
		// This is due to the template deduction guide choosing the pointer deduction rather than the reference one...
		static std::string items9[]{ "Mezzanine", "Proscenium", "ensemble", "sitzprobe", "wandelprobe", "Macbeth", "Hamlet", "Othello", "A Midsummer Night's Dream", "shook", "speared" };
        static ImGui::ComboAutoSelectData combo_data6(&items9, item_getter2); // By pointer
        if (ImGui::ComboAutoSelect("Combo 10", combo_data6, ImGuiComboFlags_NoPreview)) {
            /* Selection made */
        }
#endif __ENABLE_COMBOAUTOSELECT_HELPER__
    }
	ImGui::End();
}
