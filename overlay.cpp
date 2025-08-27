#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/TextEditor.h"
#include <dwmapi.h>
#include <filesystem>
#include <thread>
#include <bitset>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#ifdef min
#undef min
#endif
#include <stack>
#include "../../util/notification/notification.h"
#ifdef max
#undef max
#endif
#include <Psapi.h>
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/misc/freetype/imgui_freetype.h"
#include "../imgui/addons/imgui_addons.h"
#include <dwmapi.h>
#include <d3dx11.h>
#include "../../util/globals.h"
#include "keybind/keybind.h"
#include "../../features/visuals/visuals.h"
#include "../../util/config/configsystem.h"
#include "../../features/combat/modules/dahood/autostuff/auto.h"
using namespace ImGui;
using namespace ImAdd;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winhttp.lib")
namespace gui {
	static void checkbox(const char* title, bool* reference) {
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 1));
		ImGui::Checkbox(title, reference);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}
}
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
bool fullsc(HWND windowHandle);
void movewindow(HWND hw);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static ConfigSystem g_config_system;
bool isAutoFunctionActivez() {
	return globals::bools::bring || globals::bools::kill || globals::bools::autokill;
}
bool shouldTargetHudBeActive() {
	return (globals::focused && globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) ||
		(globals::focused && globals::combat::aimbot && globals::combat::aimbotkeybind.enabled) ||
		isAutoFunctionActivez();
}
void render_target_hud() {
	if (!globals::misc::targethud) return;
	if (!shouldTargetHudBeActive() && !overlay::visible) return;
	roblox::player target;
	bool hasTarget = false;
	if (overlay::visible) {
		target = globals::instances::lp;
		hasTarget = true;
	}
	else {
		if (globals::instances::cachedtarget.head.address != 0) {
			target = globals::instances::cachedtarget;
			hasTarget = true;
		}
		else if (globals::instances::cachedlasttarget.head.address != 0) {
			target = globals::instances::cachedlasttarget;
			hasTarget = true;
		}
	}
	if (!hasTarget || target.name.empty()) return;
	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;
	float rounded = style.WindowRounding;
	style.WindowRounding = 0;
	// Apply watermark styling
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowShadow, ImVec4(0.f, 0.f, 0.f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
	ImGui::PushFontShadow(IM_COL32(0, 0, 0, 255));
	static ImVec2 targethudpos = ImVec2(GetSystemMetrics(SM_CXSCREEN) / 2 - 90, GetSystemMetrics(SM_CYSCREEN) / 2 + 120);
	static bool first_time = true;
	static bool isDragging = false;
	static ImVec2 dragDelta;
	static float animatedHealth = 100.0f;
	static int lastHealth = 100;
	static float animationTimer = 0.0f;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration;
	if (!overlay::visible) {
		window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	}
	if (first_time || !overlay::visible) {
		ImGui::SetNextWindowPos(targethudpos, ImGuiCond_Always);
		first_time = false;
	}
	int health = target.humanoid.read_health();
	int maxHealth = target.humanoid.read_maxhealth();
	if (maxHealth <= 0) maxHealth = 100;
	if (health < 0) health = 0;
	if (lastHealth != health) {
		if (health < lastHealth) {
			animationTimer = 1.0f;
		}
		lastHealth = health;
	}
	float targetHealthPercentage = std::clamp(static_cast<float>(health) / maxHealth, 0.0f, 1.0f);
	float currentHealthPercentage = std::clamp(animatedHealth / maxHealth, 0.0f, 1.0f);
	if (animationTimer > 0.0f) {
		animationTimer = std::max(0.0f, animationTimer - ImGui::GetIO().DeltaTime);
	}
	if (std::abs(currentHealthPercentage - targetHealthPercentage) > 0.001f) {
		float animationSpeed = 1.0f * ImGui::GetIO().DeltaTime;
		if (targetHealthPercentage < currentHealthPercentage) {
			currentHealthPercentage = std::max(targetHealthPercentage, currentHealthPercentage - animationSpeed);
		}
		else {
			currentHealthPercentage = std::min(targetHealthPercentage, currentHealthPercentage + animationSpeed);
		}
		animatedHealth = currentHealthPercentage * maxHealth;
	}
	else {
		currentHealthPercentage = targetHealthPercentage;
	}
	const float PADDING = 8.0f;
	float totalHeight = 50;
	ImVec2 windowSize(180, totalHeight);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
	ImGui::Begin("TargetHUD", nullptr, window_flags);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImDrawList* bgdraw = ImGui::GetBackgroundDrawList();
	ImVec2 window_pos = ImGui::GetWindowPos();
	if (overlay::visible) {
		targethudpos = window_pos;
	}
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	ImRect headerRect(targethudpos, targethudpos + windowSize);
	// Colors matching watermark
	ImU32 outline_color = IM_COL32(0x32, 0x41, 0x52, 255);
	ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	ImU32 text_color = IM_COL32(255, 255, 255, 255);
	ImU32 active_color = IM_COL32(252, 150, 86, 255);
	// Health bar color with flash effect
	ImU32 healthBarColor = top_line_color;
	if (animationTimer > 0.0f && health < lastHealth) {
		float flashIntensity = std::min(1.0f, animationTimer * 2.0f);
		healthBarColor = IM_COL32(
			static_cast<int>(252 + (255 - 252) * flashIntensity),
			static_cast<int>(150 + (255 - 150) * flashIntensity),
			static_cast<int>(86 + (255 - 86) * flashIntensity),
			255
		);
	}
	if (ImGui::IsMouseClicked(0) && headerRect.Contains(mousePos) && overlay::visible) {
		isDragging = true;
		dragDelta = mousePos - targethudpos;
	}
	if (isDragging && ImGui::IsMouseDown(0)) {
		targethudpos = mousePos - dragDelta;
		ImVec2 screenSize = ImGui::GetIO().DisplaySize;
		targethudpos.x = ImClamp(targethudpos.x, 0.0f, screenSize.x - windowSize.x);
		targethudpos.y = ImClamp(targethudpos.y, 0.0f, screenSize.y - totalHeight);
	}
	else {
		isDragging = false;
	}
	// Draw watermark-style background and outline
	draw->AddRect(window_pos, window_pos + windowSize, IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.25f);
	bgdraw->AddRectFilledMultiColor(window_pos, window_pos + windowSize,
		IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 50, 255),
		IM_COL32(30, 30, 30, 255), IM_COL32(30, 30, 30, 255));
	draw->AddRect(window_pos + ImVec2(2, 2), window_pos + ImVec2(windowSize.x - 2, 4), top_line_color, 0.0f);
	draw->AddRect(window_pos + ImVec2(2, 4), window_pos + ImVec2(windowSize.x - 2, 4), IM_COL32(0, 0, 0, 100), 0.0f);
	// Health bar
	int barwidth = static_cast<int>(windowSize.x - 60);
	int healthbarwidth = static_cast<int>(currentHealthPercentage * barwidth);
	ImVec2 healthBarBg_start = ImVec2(window_pos.x + 55, window_pos.y + 35);
	ImVec2 healthBarBg_end = ImVec2(window_pos.x + 55 + barwidth, window_pos.y + 39);
	draw->AddRectFilled(healthBarBg_start, healthBarBg_end, outline_color, 1.0f);
	if (healthbarwidth > 0) {
		draw->AddRectFilled(healthBarBg_start, ImVec2(window_pos.x + 55 + healthbarwidth, window_pos.y + 39), healthBarColor, 1.0f);
	}
	draw->AddRect(healthBarBg_start, healthBarBg_end, IM_COL32(0, 0, 0, 255), 1.0f);
	// Health text
	std::string healthText = std::to_string(health) + " / " + std::to_string(maxHealth);
	ImVec2 textSize = ImGui::CalcTextSize(healthText.c_str());
	float textX = window_pos.x + 55 + (barwidth - textSize.x) / 2;
	float textY = window_pos.y + 35 + (4 - textSize.y) / 2;
	draw->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 180), healthText.c_str());
	draw->AddText(ImVec2(textX, textY), text_color, healthText.c_str());
	// Avatar
	auto* avatar_mgr = overlay::get_avatar_manager();
	if (avatar_mgr) {
		ImTextureID avatar_texture = avatar_mgr->getAvatarTexture(target.userid.address);
		if (avatar_texture) {
			draw->AddImage(
				avatar_texture,
				window_pos + ImVec2(PADDING, PADDING),
				window_pos + ImVec2(PADDING + 40, PADDING + 40)
			);
		}
		else {
			draw->AddRectFilled(
				window_pos + ImVec2(PADDING, PADDING),
				window_pos + ImVec2(PADDING + 40, PADDING + 40),
				IM_COL32(40, 40, 40, 255),
				2.0f
			);
			draw->AddText(
				window_pos + ImVec2(PADDING + 20 - ImGui::CalcTextSize("IMG").x / 2, PADDING + 20 - ImGui::CalcTextSize("IMG").y / 2),
				IM_COL32(120, 120, 120, 255),
				"IMG"
			);
		}
	}
	else {
		draw->AddRectFilled(
			window_pos + ImVec2(PADDING, PADDING),
			window_pos + ImVec2(PADDING + 40, PADDING + 40),
			IM_COL32(40, 40, 40, 255),
			2.0f
		);
		draw->AddText(
			window_pos + ImVec2(PADDING + 20 - ImGui::CalcTextSize("IMG").x / 2, PADDING + 20 - ImGui::CalcTextSize("IMG").y / 2),
			IM_COL32(120, 120, 120, 255),
			"IMG"
		);
	}
	// Target name and tool
	std::string display_name = target.name.length() > 16 ? target.name.substr(0, 13) + "..." : target.name;
	ImGui::SetCursorPos(ImVec2(PADDING, PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(active_color), display_name.c_str());
	ImGui::PopStyleVar();
	std::string tool_name = "Combat"; // Default to "Combat" as a fallback
	try {
		auto tool = target.instance.read_service("Tool");
		if (tool.address) {
			std::string tool_str = tool.get_name();
			if (!tool_str.empty() && tool_str != "nil") {
				tool_name = tool_str.length() > 15 ? tool_str.substr(0, 12) + "..." : tool_str;
			}
		}
	}
	catch (...) {
		tool_name = "Combat"; // Fallback to "Combat" on exception
	}
	ImGui::SetCursorPos(ImVec2(PADDING, PADDING + 13));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(text_color), tool_name.c_str());
	style.WindowRounding = rounded;
	ImGui::End();
	ImGui::PopFontShadow();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}
void render_explorer() {
	if (!globals::misc::explorer) return;
	static roblox::instance selectedPart;
	static std::unordered_set<uint64_t> spectatedParts;
	static std::unordered_map<uint64_t, std::vector<roblox::instance>> nodeCache;
	static std::unordered_map<uint64_t, bool> nodeExpandedCache;
	static std::unordered_map<uint64_t, std::string> nodeNameCache;
	static std::unordered_map<uint64_t, std::string> nodeClassNameCache;
	static std::unordered_map<uint64_t, std::string> nodeTeamCache;
	static std::unordered_map<uint64_t, std::string> nodeUniqueIdCache;
	static std::unordered_map<uint64_t, std::string> nodeModel;
	static char searchQuery[128] = "";
	static std::vector<roblox::instance> searchResults;
	static bool showSearchResults = false;
	static bool isCacheInitialized = false;
	static auto lastCacheRefresh = std::chrono::steady_clock::now();
	static std::unordered_map<uint64_t, std::string> nodePath;
	static bool showProperties = true;
	static int selectedTab = 0;
	static float splitRatio = 0.65f;
	static ImVec2 explorer_pos = ImVec2(GetSystemMetrics(SM_CXSCREEN) - 680, 80);
	static bool first_time = true;
	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;
	float rounded = style.WindowRounding;
	style.WindowRounding = 0;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize;
	if (!overlay::visible) {
		window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	}
	if (first_time || !overlay::visible) {
		ImGui::SetNextWindowPos(explorer_pos, ImGuiCond_Always);
		first_time = false;
	}
	static std::unordered_map<std::string, std::string> classPrefixes = {
	{"Workspace", "[WS] "},
	{"Players", "[P] "},
	{"Folder", "[F] "},
	{"Part", "[PT] "},
	{"BasePart", "[BP] "},
	{"Script", "[S] "},
	{"LocalScript", "[LS] "},
	{"ModuleScript", "[MS] "}
	};
	auto cacheNode = [&](roblox::instance& node) {
		if (nodeCache.find(node.address) == nodeCache.end()) {
			nodeCache[node.address] = node.get_children();
			nodeNameCache[node.address] = node.get_name();
			nodeClassNameCache[node.address] = node.get_class_name();
			nodeUniqueIdCache[node.address] = std::to_string(node.address);
			std::string path = node.get_name();
			roblox::instance parent = node.read_parent();
			while (parent.address != 0) {
				if (nodeCache.find(parent.address) == nodeCache.end()) {
					nodeCache[parent.address] = parent.get_children();
					nodeNameCache[parent.address] = parent.get_name();
				}
				std::string parentName = nodeNameCache[parent.address];
				if (!parentName.empty()) {
					path = parentName + "." + path;
				}
				parent = parent.read_parent();
			}
			nodePath[node.address] = path;
		}
		};
	try {
		auto& datamodel = globals::instances::datamodel;
		roblox::instance root_instance(datamodel.address);
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheRefresh).count() >= 2) {
			nodeCache.clear();
			nodeNameCache.clear();
			nodeClassNameCache.clear();
			nodeUniqueIdCache.clear();
			isCacheInitialized = false;
			lastCacheRefresh = now;
		}
		if (!isCacheInitialized) {
			cacheNode(root_instance);
			isCacheInitialized = true;
		}
		float content_width = 650.0f;
		float padding = 8.0f;
		float header_height = 30.0f;
		float total_height = 700.0f;
		ImGui::SetNextWindowSize(ImVec2(content_width + (padding * 2), total_height), ImGuiCond_Always);
		ImGui::Begin("Explorer", nullptr, window_flags);
		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (overlay::visible) {
			explorer_pos = window_pos;
		}
		ImU32 text_color = IM_COL32(255, 255, 255, 255);
		ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
		draw->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 2), top_line_color, 0.0f);
		draw->AddText(ImVec2(window_pos.x + padding, window_pos.y + 8), text_color, "Explorer");
		ImGui::SetCursorPos(ImVec2(padding, header_height));
		ImGui::PushItemWidth(content_width - 100);
		bool searchChanged = ImGui::InputTextWithHint("##Search", "Search...", searchQuery, IM_ARRAYSIZE(searchQuery), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::PopItemWidth();
		if (searchChanged) {
			searchResults.clear();
			showSearchResults = strlen(searchQuery) > 0;
			if (showSearchResults && strlen(searchQuery) >= 1) {
				std::string query = searchQuery;
				std::transform(query.begin(), query.end(), query.begin(), ::tolower);
				std::function<void(roblox::instance&)> searchInstance = [&](roblox::instance& instance) {
					if (searchResults.size() >= 100) return;
					cacheNode(instance);
					std::string name = nodeNameCache[instance.address];
					std::transform(name.begin(), name.end(), name.begin(), ::tolower);
					if (name.find(query) != std::string::npos) {
						searchResults.push_back(instance);
					}
					for (auto& child : instance.get_children()) {
						searchInstance(child);
					}
					};
				if (auto workspace = root_instance.findfirstchild("Workspace"); workspace.address != 0) {
					searchInstance(workspace);
				}
				if (globals::instances::players.address != 0) {
					for (auto& player : globals::instances::players.get_children()) {
						searchInstance(player);
					}
				}
				if (auto teams = root_instance.findfirstchild("Teams"); teams.address != 0) {
					searchInstance(teams);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh", ImVec2(70, 0))) {
			nodeCache.clear();
			nodeNameCache.clear();
			nodeClassNameCache.clear();
			nodeUniqueIdCache.clear();
			isCacheInitialized = false;
			searchResults.clear();
			showSearchResults = false;
			memset(searchQuery, 0, sizeof(searchQuery));
		}
		float treeHeight = 400.0f;
		ImGui::BeginChild("ExplorerTree", ImVec2(content_width, treeHeight), true);
		if (showSearchResults && strlen(searchQuery) > 0) {
			ImGui::Text("Search Results (%d):", static_cast<int>(searchResults.size()));
			ImGui::Separator();
			if (!searchResults.empty()) {
				for (auto& node : searchResults) {
					if (!node.address) continue;
					if (nodeCache.find(node.address) == nodeCache.end()) {
						cacheNode(node);
					}
					ImGui::PushID(nodeUniqueIdCache[node.address].c_str());
					std::string displayText = nodeNameCache[node.address];
					std::string className = nodeClassNameCache[node.address];
					std::string fullText = displayText + " [" + className + "]";
					bool is_selected = (selectedPart.address == node.address);
					if (ImGui::Selectable(fullText.c_str(), is_selected)) {
						selectedPart = node;
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
						ImGui::OpenPopup("NodeContextMenu");
						selectedPart = node;
					}
					if (ImGui::BeginPopup("NodeContextMenu")) {
						ImGui::Text("Operations for: %s", displayText.c_str());
						ImGui::Separator();
						if (ImGui::MenuItem("Copy Address")) {
							ImGui::SetClipboardText(nodeUniqueIdCache[node.address].c_str());
						}
						if (ImGui::MenuItem("Set Collision True")) {
							selectedPart.write_cancollide(true);
						}
						if (ImGui::MenuItem("Set Collision False")) {
							selectedPart.write_cancollide(false);
						}
						if (ImGui::MenuItem("Teleport To Part")) {
							Vector3 partPosition = node.get_pos();
							const float verticalOffset = 5.0f;
							partPosition.y += verticalOffset;
							globals::instances::lp.hrp.write_position(partPosition);
						}
						if (spectatedParts.count(node.address)) {
							if (ImGui::MenuItem("Stop Spectating")) {
								selectedPart.unspectate();
								spectatedParts.erase(node.address);
							}
						}
						else {
							if (ImGui::MenuItem("Spectate Part")) {
								selectedPart = node;
								selectedPart.spectate(node.address);
								spectatedParts.insert(node.address);
							}
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}
			else {
				ImGui::Text("No results found");
			}
		}
		else {
			for (auto& child : nodeCache[root_instance.address]) {
				std::stack<std::pair<roblox::instance, int>> stack;
				stack.push({ child, 0 });
				while (!stack.empty()) {
					auto [node, indentLevel] = stack.top();
					stack.pop();
					cacheNode(node);
					ImGui::SetCursorPosX(20.0f * indentLevel);
					ImGui::PushID(nodeUniqueIdCache[node.address].c_str());
					const std::vector<roblox::instance>& children = nodeCache[node.address];
					bool hasChildren = !children.empty();
					std::string className = nodeClassNameCache[node.address];
					std::string prefix = "";
					if (classPrefixes.find(className) != classPrefixes.end()) {
						prefix = classPrefixes[className];
					}
					std::string displayText = prefix + nodeNameCache[node.address] + " [" + className + "]";
					ImGuiTreeNodeFlags flags = hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;
					flags |= ImGuiTreeNodeFlags_OpenOnArrow;
					if (selectedPart.address == node.address) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}
					bool isExpanded = ImGui::TreeNodeEx(displayText.c_str(), flags);
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
						selectedPart = node;
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
						ImGui::OpenPopup("NodeContextMenu");
						selectedPart = node;
					}
					if (ImGui::BeginPopup("NodeContextMenu")) {
						ImGui::Text("%s", nodeNameCache[node.address].c_str());
						ImGui::Separator();
						if (ImGui::MenuItem("Copy Address")) {
							ImGui::SetClipboardText(nodeUniqueIdCache[node.address].c_str());
						}
						if (ImGui::MenuItem("Set Collision True")) {
							selectedPart.write_cancollide(true);
						}
						if (ImGui::MenuItem("Set Collision False")) {
							selectedPart.write_cancollide(false);
						}
						if (ImGui::MenuItem("Teleport To Part")) {
							Vector3 partPosition = node.get_pos();
							const float verticalOffset = 5.0f;
							partPosition.y += verticalOffset;
							globals::instances::lp.hrp.write_position(partPosition);
						}
						if (spectatedParts.count(node.address)) {
							if (ImGui::MenuItem("Stop Spectating")) {
								selectedPart.unspectate();
								spectatedParts.erase(node.address);
							}
						}
						else {
							if (ImGui::MenuItem("Spectate Part")) {
								selectedPart = node;
								selectedPart.spectate(node.address);
								spectatedParts.insert(node.address);
							}
						}
						ImGui::EndPopup();
					}
					if (isExpanded) {
						for (auto it = children.rbegin(); it != children.rend(); ++it) {
							stack.push({ *it, indentLevel + 1 });
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
		ImGui::BeginChild("Properties", ImVec2(content_width, 240), true);
		ImGui::Text("Properties");
		ImGui::Separator();
		if (selectedPart.address != 0) {
			if (ImGui::BeginTabBar("PropertiesTabBar")) {
				if (ImGui::BeginTabItem("Basic")) {
					ImGui::BeginChild("BasicScrollRegion", ImVec2(0, 160), false);
					const std::string& nodeName = nodeNameCache[selectedPart.address];
					const std::string& className = nodeClassNameCache[selectedPart.address];
					roblox::instance parent = selectedPart.read_parent();
					std::string parentName = nodeNameCache[parent.address];
					float col1Width = 120.0f;
					ImGui::Text("Path:");
					ImGui::SameLine(col1Width);
					ImGui::TextWrapped("%s", nodePath[selectedPart.address].c_str());
					ImGui::Text("Name:");
					ImGui::SameLine(col1Width);
					ImGui::Text("%s", nodeName.c_str());
					ImGui::Text("Class:");
					ImGui::SameLine(col1Width);
					ImGui::Text("%s", className.c_str());
					ImGui::Text("Parent:");
					ImGui::SameLine(col1Width);
					ImGui::Text("%s", parentName.c_str());
					ImGui::Text("Address:");
					ImGui::SameLine(col1Width);
					ImGui::Text("0x%llX", selectedPart.address);
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Transform")) {
					ImGui::BeginChild("TransformScrollRegion", ImVec2(0, 160), false);
					float col1Width = 120.0f;
					Vector3 position = selectedPart.get_pos();
					ImGui::Text("Position:");
					ImGui::SameLine(col1Width);
					ImGui::Text("X: %.2f Y: %.2f Z: %.2f", position.x, position.y, position.z);
					Vector3 size = selectedPart.get_part_size();
					ImGui::Text("Size:");
					ImGui::SameLine(col1Width);
					ImGui::Text("X: %.2f Y: %.2f Z: %.2f", size.x, size.y, size.z);
					ImGui::Separator();
					ImGui::Text("Quick Actions:");
					if (ImGui::Button("Teleport To", ImVec2(120, 25))) {
						Vector3 partPosition = selectedPart.get_pos();
						const float verticalOffset = 5.0f;
						partPosition.y += verticalOffset;
						globals::instances::lp.hrp.write_position(partPosition);
					}
					ImGui::SameLine();
					if (ImGui::Button("Toggle Collide", ImVec2(120, 25))) {
						bool currentState = selectedPart.get_cancollide();
						selectedPart.write_cancollide(!currentState);
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}
		else {
			ImGui::Text("No object selected");
		}
		ImGui::EndChild();
		ImGui::Separator();
		int totalNodes = nodeCache.size();
		int spectatingCount = spectatedParts.size();
		ImGui::Text("Nodes: %d | Spectating: %d", totalNodes, spectatingCount);
		ImGui::End();
		style.WindowRounding = rounded;
	}
	catch (const std::exception& e) {
		ImGui::Text("Error: %s", e.what());
	}
	catch (...) {
		ImGui::Text("Unknown error occurred");
	}
}
void overlay::initialize_avatar_system() {
	if (g_pd3dDevice && !avatar_manager) {
		avatar_manager = std::make_unique<AvatarManager>(g_pd3dDevice, g_pd3dDeviceContext);
	}
}
void overlay::update_avatars() {
	if (avatar_manager) {
		avatar_manager->update();
	}
}
AvatarManager* overlay::get_avatar_manager() {
	return avatar_manager.get();
}
void overlay::cleanup_avatar_system() {
	if (avatar_manager) {
		avatar_manager.reset();
	}
}
void render_player_list() {
	static ImVec2 playerlist_pos = ImVec2(500, 10);
	static bool first_time = true;
	static int selected_player = -1;
	static std::vector<std::string> status_options = { "Enemy", "Friendly", "Neutral", "Client" };
	static std::vector<int> player_status;
	ImGuiStyle& style = ImGui::GetStyle();
	float original_rounding = style.WindowRounding;
	style.WindowRounding = 0;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
	if (!overlay::visible) window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	if (first_time || !overlay::visible) {
		ImGui::SetNextWindowPos(playerlist_pos, ImGuiCond_Always);
		first_time = false;
	}
	std::vector<roblox::player> players = globals::instances::cachedplayers;
	if (player_status.size() != players.size()) {
		player_status.resize(players.size(), 2);
		for (size_t i = 0; i < players.size(); i++) {
			auto& player = players[i];
			if (player.name == globals::instances::lp.name) player_status[i] = 3;
			else if (std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), player.name) != globals::instances::whitelist.end())
				player_status[i] = 1;
			else if (std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), player.name) != globals::instances::blacklist.end())
				player_status[i] = 0;
		}
	}
	//ImGui::Begin("Player List", nullptr, window_flags);
	if (overlay::visible) playerlist_pos = ImGui::GetWindowPos();
	ImGui::Text("Players (%d)", (int)players.size());
	ImGui::Separator();
	ImGui::Columns(2, nullptr, false);
	ImGui::SetColumnWidth(0, 200);
	ImGui::Text("Name"); ImGui::NextColumn();
	ImGui::Text("Status"); ImGui::NextColumn();
	ImGui::Separator();
	for (size_t i = 0; i < players.size(); ++i) {
		auto& player = players[i];
		ImGui::PushID((int)i);
		bool is_selected = (selected_player == static_cast<int>(i));
		if (ImGui::Selectable(player.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
			selected_player = (selected_player == static_cast<int>(i)) ? -1 : static_cast<int>(i);
		}
		ImGui::NextColumn();
		const char* status = "Neutral";
		switch (player_status[i]) {
		case 0: status = "Enemy"; break;
		case 1: status = "Friendly"; break;
		case 2: status = "Neutral"; break;
		case 3: status = "Client"; break;
		}
		ImGui::Text("%s", status);
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);
	ImGui::Separator();
	static bool spectating = false;
	if (selected_player >= 0 && selected_player < (int)players.size()) {
		auto& selected_ply = players[selected_player];
		bool is_client = selected_ply.name == globals::instances::lp.name;
		ImGui::Text("Selected: %s", selected_ply.name.c_str());
		if (!is_client) {
			Vector3 pos = selected_ply.hrp.get_pos();
			int health = selected_ply.humanoid.read_health();
			int maxhealth = selected_ply.humanoid.read_maxhealth();
			ImGui::Text("Position: X:%d Y:%d Z:%d", (int)pos.x, (int)pos.y, (int)pos.z);
			ImGui::Text("Health: %d / %d", health, maxhealth);
			float w = ImGui::GetContentRegionAvail().x / 3.0f;
			if (ImGui::Button(spectating ? "Unspectate" : "Spectate", ImVec2(w, 0))) {
				roblox::instance cam;
				if (spectating) {
					cam.unspectate();
					spectating = false;
				}
				else {
					cam.spectate(selected_ply.hrp.address);
					spectating = true;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Kill", ImVec2(w, 0))) {
				globals::bools::name = selected_ply.name;
				globals::bools::entity = selected_ply;
				globals::bools::kill = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Bring", ImVec2(w, 0))) {
				globals::bools::name = selected_ply.name;
				globals::bools::entity = selected_ply;
				globals::bools::bring = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Teleport", ImVec2(w, 0))) {
				globals::instances::lp.hrp.write_position(selected_ply.hrp.get_pos());
			}
			ImGui::Text("Status:");
			ImGui::SameLine();
			std::string current_status = player_status[selected_player] < status_options.size() ?
				status_options[player_status[selected_player]] : "Neutral";
			if (ImGui::BeginCombo("##status", current_status.c_str())) {
				for (int i = 0; i < 3; i++) {
					bool is_selected_status = player_status[selected_player] == i;
					if (ImGui::Selectable(status_options[i].c_str(), is_selected_status)) {
						player_status[selected_player] = i;
						if (i == 0) {
							if (std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), selected_ply.name) == globals::instances::blacklist.end())
								globals::instances::blacklist.push_back(selected_ply.name);
							globals::instances::whitelist.erase(std::remove(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), selected_ply.name), globals::instances::whitelist.end());
						}
						else if (i == 1) {
							if (std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), selected_ply.name) == globals::instances::whitelist.end())
								globals::instances::whitelist.push_back(selected_ply.name);
							globals::instances::blacklist.erase(std::remove(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), selected_ply.name), globals::instances::blacklist.end());
						}
						else {
							globals::instances::whitelist.erase(std::remove(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), selected_ply.name), globals::instances::whitelist.end());
							globals::instances::blacklist.erase(std::remove(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), selected_ply.name), globals::instances::blacklist.end());
						}
					}
					if (is_selected_status) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "LocalPlayer");
			ImGui::Text("Status: Client");
		}
	}
	else {
		ImGui::Text("No player selected.");
		ImGui::Text("Click a player to see options.");
	}
	ImGui::End();
	style.WindowRounding = original_rounding;
}
void render_watermark() {
	if (!globals::misc::watermark) return;
	unsigned int accent_color_u32 = IM_COL32(255, 132, 56, 255);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowShadow, ImVec4(0.f, 0.f, 0.f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
	ImGui::PushFontShadow(IM_COL32(0, 0, 0, 255));
	std::stringstream ss;
	ss << " | fps: " << static_cast<int>(ImGui::GetIO().Framerate);
	ss << " | welcome: " << globals::instances::username;
	time_t now = time(0);
	tm ltm;
	localtime_s(&ltm, &now);
	int month = ltm.tm_mon;
	int year = 1900 + ltm.tm_year;
	std::string month_str;
	switch (month) {
	case 0: month_str = "january"; break;
	case 1: month_str = "february"; break;
	case 2: month_str = "march"; break;
	case 3: month_str = "april"; break;
	case 4: month_str = "may"; break;
	case 5: month_str = "june"; break;
	case 6: month_str = "july"; break;
	case 7: month_str = "august"; break;
	case 8: month_str = "september"; break;
	case 9: month_str = "october"; break;
	case 10: month_str = "november"; break;
	case 11: month_str = "december"; break;
	}
	ss << " | " << month_str << " " << ltm.tm_mday << ", " << year;
	std::stringstream watermark_text_str;
	watermark_text_str << "medusa.lol" << ss.str().c_str();
	ImGui::SetNextWindowPos(ImVec2(5, 5));
	ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(watermark_text_str.str().c_str()).x + 16, 25));
	ImGui::Begin("##watermark", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
	auto bgdraw = ImGui::GetBackgroundDrawList();
	auto draw = ImGui::GetForegroundDrawList();
	auto window_draw = ImGui::GetWindowDrawList();
	auto size = ImGui::GetWindowSize();
	auto pos = ImGui::GetWindowPos();
	draw->AddRect(pos, pos + size, IM_COL32(0, 0, 0, 255), 0.0F, 0, 1.25f);
	bgdraw->AddRectFilledMultiColor(pos, pos + size, IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 50, 255), IM_COL32(30, 30, 30, 255), IM_COL32(30, 30, 30, 255));
	draw->AddRect(pos + ImVec2(2, 2), pos + ImVec2(size.x - 2, 4), ImGui::GetColorU32(accent_color_u32));
	draw->AddRect(pos + ImVec2(2, 4), pos + ImVec2(size.x - 2, 4), ImColor(0.f, 0.f, 0.f, ImClamp(100 / 255.f, 0.f, 1.f)));
	ImGui::SetCursorPos(ImVec2(8, 8));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(accent_color_u32)), "medusa");
	ImGui::SameLine();
	ImGui::Text(".lol%s", ss.str().c_str());
	ImGui::PopStyleVar();
	ImGui::End();
	ImGui::PopFontShadow();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}
void render_keybind_list() {
	if (!globals::misc::keybinds) return;
	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;
	// Store original window rounding and set to 0 to match watermark
	float rounded = style.WindowRounding;
	style.WindowRounding = 0;
	// Push watermark-like styling
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowShadow, ImVec4(0.f, 0.f, 0.f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
	ImGui::PushFontShadow(IM_COL32(0, 0, 0, 255));
	static ImVec2 keybind_pos = ImVec2(5, GetSystemMetrics(SM_CYSCREEN) / 2 - 10);
	static bool first_time = true;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
	if (!overlay::visible) {
		window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	}
	if (first_time || (!overlay::visible)) {
		ImGui::SetNextWindowPos(keybind_pos, ImGuiCond_Always);
		first_time = false;
	}
	std::vector<std::pair<std::string, std::string>> active_keybinds;
	if (globals::combat::aimbot && globals::combat::aimbotkeybind.enabled) {
		active_keybinds.push_back({ "Aimbot", globals::combat::aimbotkeybind.get_key_name() });
	}
	if (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) {
		active_keybinds.push_back({ "Silent Aim", globals::combat::silentaimkeybind.get_key_name() });
	}
	if (globals::combat::orbit && globals::combat::orbitkeybind.enabled) {
		active_keybinds.push_back({ "Orbit", globals::combat::orbitkeybind.get_key_name() });
	}
	if (globals::combat::triggerbot && globals::combat::triggerbotkeybind.enabled) {
		active_keybinds.push_back({ "TriggerBot", globals::combat::triggerbotkeybind.get_key_name() });
	}
	if (globals::misc::speed && globals::misc::speedkeybind.enabled) {
		active_keybinds.push_back({ "Speed", globals::misc::speedkeybind.get_key_name() });
	}
	if (globals::misc::jumppower && globals::misc::jumppowerkeybind.enabled) {
		active_keybinds.push_back({ "Jump Power", globals::misc::jumppowerkeybind.get_key_name() });
	}
	if (globals::misc::flight && globals::misc::flightkeybind.enabled) {
		active_keybinds.push_back({ "Flight", globals::misc::flightkeybind.get_key_name() });
	}
	if (globals::misc::voidhide && globals::misc::voidhidebind.enabled) {
		active_keybinds.push_back({ "Void Hide", globals::misc::voidhidebind.get_key_name() });
	}
	if (globals::misc::autostomp && globals::misc::stompkeybind.enabled) {
		active_keybinds.push_back({ "Auto Stomp", globals::misc::stompkeybind.get_key_name() });
	}
	if (globals::misc::keybindsstyle == 1) {
		struct KeybindInfo {
			std::string name;
			keybind* bind;
			bool* enabled;
		};
		std::vector<KeybindInfo> all_keybinds = {
		{"Aimbot", &globals::combat::aimbotkeybind, &globals::combat::aimbot},
		{"Silent Aim", &globals::combat::silentaimkeybind, &globals::combat::silentaim},
		{"Orbit", &globals::combat::orbitkeybind, &globals::combat::orbit},
		{"TriggerBot", &globals::combat::triggerbotkeybind, &globals::combat::triggerbot},
		{"Speed", &globals::misc::speedkeybind, &globals::misc::speed},
		{"Jump Power", &globals::misc::jumppowerkeybind, &globals::misc::jumppower},
		{"Flight", &globals::misc::flightkeybind, &globals::misc::flight},
		{"Void Hide", &globals::misc::voidhidebind, &globals::misc::voidhide},
		{"Auto Stomp", &globals::misc::stompkeybind, &globals::misc::autostomp}
		};
		active_keybinds.clear();
		for (const auto& info : all_keybinds) {
			if (*info.enabled) {
				active_keybinds.push_back({ info.name, info.bind->get_key_name() });
			}
		}
	}
	ImVec2 title_size = ImGui::CalcTextSize("Keybinds");
	float content_width = title_size.x;
	for (const auto& bind : active_keybinds) {
		std::string full_text = bind.first + ": " + bind.second;
		ImVec2 text_size = ImGui::CalcTextSize(full_text.c_str());
		content_width = std::max(content_width, text_size.x);
	}
	float padding_x = 8.0f; // Match watermark's text padding
	float padding_y = 8.0f; // Match watermark's text padding
	float line_spacing = ImGui::GetTextLineHeight() + 2.0f;
	float total_width = content_width + (padding_x * 2);
	float total_height = padding_y * 2 + title_size.y + 2;
	if (!active_keybinds.empty()) {
		total_height += active_keybinds.size() * line_spacing;
	}
	ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
	ImGui::Begin("Keybinds", nullptr, window_flags | ImGuiWindowFlags_NoBackground);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImDrawList* bgdraw = ImGui::GetBackgroundDrawList();
	ImVec2 window_pos = ImGui::GetWindowPos();
	ImVec2 window_size = ImGui::GetWindowSize();
	if (overlay::visible) {
		keybind_pos = window_pos;
	}
	// Colors matching watermark
	ImU32 bg_color = IM_COL32(0.078f * 255, 0.078f * 255, 0.078f * 255, 200);
	ImU32 text_color = IM_COL32(255, 255, 255, 255);
	ImU32 outline_color = IM_COL32(0x32, 0x41, 0x52, 255);
	ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	ImU32 active_color = IM_COL32(0.988f * 255, 0.588f * 255, 0.337f * 255, 255);
	// Draw background and outline to match watermark
	draw->AddRect(window_pos, window_pos + window_size, outline_color, 0.0f, 0, 1.25f);
	bgdraw->AddRectFilledMultiColor(window_pos, window_pos + window_size,
		IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 50, 255),
		IM_COL32(30, 30, 30, 255), IM_COL32(30, 30, 30, 255));
	draw->AddRect(window_pos + ImVec2(2, 2), window_pos + ImVec2(window_size.x - 2, 4), top_line_color);
	draw->AddRect(window_pos + ImVec2(2, 4), window_pos + ImVec2(window_size.x - 2, 4), IM_COL32(0, 0, 0, 100));
	ImVec2 title_pos = ImVec2(window_pos.x + padding_x, window_pos.y + padding_y);
	// Draw title with accent color to match "medusa" in watermark
	ImGui::SetCursorPos(ImVec2(8, 8));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(top_line_color), "Keybinds");
	ImGui::PopStyleVar();
	if (!active_keybinds.empty()) {
		float current_y = title_pos.y + title_size.y + 4.0f;
		std::sort(active_keybinds.begin(), active_keybinds.end(),
			[](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
				std::string full_a = a.first + ": " + a.second;
				std::string full_b = b.first + ": " + b.second;
				return full_a.length() > full_b.length();
			});
		for (const auto& bind : active_keybinds) {
			std::string full_text = bind.first + ": " + bind.second;
			ImVec2 keybind_pos = ImVec2(window_pos.x + padding_x, current_y);
			if (globals::misc::keybindsstyle == 1) {
				bool is_active = false;
				if (bind.first == "Aimbot") is_active = globals::combat::aimbotkeybind.enabled;
				else if (bind.first == "Silent Aim") is_active = globals::combat::silentaimkeybind.enabled;
				else if (bind.first == "Orbit") is_active = globals::combat::orbitkeybind.enabled;
				else if (bind.first == "TriggerBot") is_active = globals::combat::triggerbotkeybind.enabled;
				else if (bind.first == "Speed") is_active = globals::misc::speedkeybind.enabled;
				else if (bind.first == "Jump Power") is_active = globals::misc::jumppowerkeybind.enabled;
				else if (bind.first == "Flight") is_active = globals::misc::flightkeybind.enabled;
				else if (bind.first == "Void Hide") is_active = globals::misc::voidhidebind.enabled;
				else if (bind.first == "Auto Stomp") is_active = globals::misc::stompkeybind.enabled;
				draw->AddText(keybind_pos, is_active ? active_color : text_color, full_text.c_str());
			}
			else {
				draw->AddText(keybind_pos, text_color, full_text.c_str());
			}
			current_y += line_spacing;
		}
	}
	style.WindowRounding = rounded;
	ImGui::End();
	ImGui::PopFontShadow();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}
bool Bind(keybind* keybind, const ImVec2& size_arg = ImVec2(0, 0), bool clicked = false, ImGuiButtonFlags flags = 0) {
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;
	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(keybind->get_name().c_str());
	const ImVec2 label_size =
		ImGui::CalcTextSize(keybind->get_name().c_str(), NULL, true);
	ImVec2 pos = window->DC.CursorPos;
	if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
		style.FramePadding.y <
		window->DC
		.CurrLineTextBaseOffset)
		pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
	ImVec2 size =
		CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
			label_size.y + style.FramePadding.y * 2.0f);
	const ImRect bb(pos, pos + size);
	ItemSize(size, style.FramePadding.y);
	if (!ItemAdd(bb, id))
		return false;
	if (g.CurrentItemFlags & ImGuiItemFlags_ButtonRepeat)
		flags |= ImGuiButtonFlags_Repeat;
	bool hovered, held;
	bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);
	bool value_changed = false;
	int key = keybind->key;
	auto io = ImGui::GetIO();
	std::string name = keybind->get_key_name();
	if (keybind->waiting_for_input)
		name = "waiting";
	if (ImGui::GetIO().MouseClicked[0] && hovered) {
		if (g.ActiveId == id) {
			keybind->waiting_for_input = true;
		}
	}
	else if (ImGui::GetIO().MouseClicked[1] && hovered) {
		OpenPopup(keybind->get_name().c_str());
	}
	else if (ImGui::GetIO().MouseClicked[0] && !hovered) {
		if (g.ActiveId == id)
			ImGui::ClearActiveID();
	}
	if (keybind->waiting_for_input)
		if (keybind->set_key()) {
			ImGui::ClearActiveID();
			keybind->waiting_for_input = false;
		}
	// Render
	ImVec4 textcolor = ImLerp(ImVec4(201 / 255.f, 204 / 255.f, 210 / 255.f, 1.f), ImVec4(1.0f, 1.0f, 1.0f, 1.f), 1.f);
	window->DrawList->AddRectFilled(bb.Min, bb.Max, ImColor(33 / 255.0f, 33 / 255.0f, 33 / 255.0f, 1.f));
	window->DrawList->AddRect(bb.Min, bb.Max, ImColor(0.f, 0.f, 0.f, 1.f));
	window->DrawList->AddText(bb.Min + ImVec2(size_arg.x / 2 - CalcTextSize(name.c_str()).x / 2, size_arg.y / 2 - CalcTextSize(name.c_str()).y / 2), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)), name.c_str());
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
	SetNextWindowPos(pos + ImVec2(0, size_arg.y - 1));
	SetNextWindowSize(ImVec2(72, 60 * 1.f));
	{
		if (BeginPopup(keybind->get_name().c_str(), window_flags)) {
			PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
			{
				SetCursorPos(ImVec2(7, 2));
				{
					BeginGroup();
					{
						if (ImAdd::Selectable("always on", keybind->type == keybind::ALWAYS, ImVec2(-0.1f, 0)))
							keybind->type = keybind::ALWAYS;
						if (ImAdd::Selectable("hold", keybind->type == keybind::HOLD, ImVec2(-0.1f, 0)))
							keybind->type = keybind::HOLD;
						if (ImAdd::Selectable("toggle", keybind->type == keybind::TOGGLE, ImVec2(-0.1f, 0)))
							keybind->type = keybind::TOGGLE;
					}
					EndGroup();
				}
			}
			PopStyleVar();
			EndPopup();
		}
	}
	return pressed;
}




void draw_shadowed_text(const char* label) {
	ImGuiContext& g = *GImGui;
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	const ImGuiStyle& style = g.Style;
	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();

	// Use the custom font you loaded
	ImFont* customFont = io.Fonts->Fonts[0]; // Assuming the custom font is loaded as the second font

	float HeaderHeight = customFont->FontSize + style.WindowPadding.y * 2 + style.ChildBorderSize * 2;
	pos.y = pos.y - 4;

	// Draw shadowed text using the custom font
	pDrawList->AddText(customFont, customFont->FontSize, pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
	pDrawList->AddText(customFont, customFont->FontSize, pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
	pDrawList->AddText(customFont, customFont->FontSize, pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
	pDrawList->AddText(customFont, customFont->FontSize, pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
	pDrawList->AddText(customFont, customFont->FontSize, pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2), ImGui::GetColorU32(ImGuiCol_Text), label);

	ImGui::SetCursorPosY(HeaderHeight - style.WindowPadding.y + 2);
}
ImU32 ColorConvert(float in[4])
{
	auto v = ImVec4(in[0], in[1], in[2], in[3]);
	return ImGui::ColorConvertFloat4ToU32(v);
}
void overlay::load_interface()
{
	ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&wc);
	HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"SUNK OVERLAY", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);
	wc.cbClsExtra = NULL;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbWndExtra = NULL;
	wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = TEXT(L"base");
	wc.lpszMenuName = nullptr;
	wc.style = CS_VREDRAW | CS_HREDRAW;
	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
	MARGINS margin = { -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margin);
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return;
	}
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	bool fadeIn = true;
	bool fadeOut = false;
	float alpha = 1.f;
	float fadeSpeed = 0.04f; // Adjust as needed
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4 accent = ImColor(0.988f, 0.588f, 0.337f, alpha);
	style.Colors[ImGuiCol_WindowBg] = ImColor(0.078f, 0.078f, 0.078f, alpha);
	style.Colors[ImGuiCol_FrameBg] = ImColor(0.118f, 0.118f, 0.118f, alpha);
	style.Colors[ImGuiCol_FrameBgActive] = ImColor(0.098f, 0.098f, 0.098f, alpha);
	style.Colors[ImGuiCol_FrameBgHovered] = ImColor(0.361f, 0.361f, 0.361f, alpha);
	style.Colors[ImGuiCol_Header] = ImColor(1.f, 0.518f, 0.22f, alpha);
	style.Colors[ImGuiCol_HeaderHovered] = ImColor(0.549f, 0.306f, 0.157f, alpha);
	style.Colors[ImGuiCol_HeaderActive] = ImColor(0.349f, 0.192f, 0.098f, alpha);
	style.Colors[ImGuiCol_Button] = ImColor(0.118f, 0.118f, 0.118f, alpha);
	style.Colors[ImGuiCol_ButtonHovered] = ImColor(1.f, 0.518f, 0.22f, alpha);
	style.Colors[ImGuiCol_ButtonActive] = ImColor(0.349f, 0.192f, 0.098f, alpha);
	style.Colors[ImGuiCol_ResizeGrip] = ImColor(0.059f, 0.059f, 0.059f, alpha);
	style.Colors[ImGuiCol_ResizeGripActive] = ImColor(0.059f, 0.059f, 0.059f, alpha);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImColor(0.059f, 0.059f, 0.059f, alpha);
	style.Colors[ImGuiCol_WindowShadow] = ImColor(0.098f, 0.098f, 0.098f, alpha);
	style.Colors[ImGuiCol_TitleBg] = ImColor(0.f, 0.f, 0.f, alpha);
	style.Colors[ImGuiCol_Text] = ImColor(1.f, 1.f, 1.f, alpha);
	style.Colors[ImGuiCol_CheckMark] = accent;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accent;
	style.Colors[ImGuiCol_Separator] = accent;
	style.Colors[ImGuiCol_SeparatorActive] = accent;
	style.WindowBorderSize = 2;
	style.Colors[ImGuiCol_Border] = ImAdd::HexToColorVec4(0x324152, 1);
	ImFontConfig cfg;
	cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
	cfg.PixelSnapH = true;
	cfg.SizePixels = 12.0f;
	cfg.RasterizerMultiply = 1.0f;
	char windows_directory[MAX_PATH];
	GetWindowsDirectoryA(windows_directory, MAX_PATH);
	std::string tahoma_font_directory = (std::string)windows_directory + ("\\Fonts\\tahoma.ttf");
	ImFont* tahoma = io.Fonts->AddFontFromFileTTF(tahoma_font_directory.c_str(), 12.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
	ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
	bool done = false;
	initialize_avatar_system();
	while (done == false)
	{
		if (!globals::firstreceived)return;

		auto avatar_mgr = overlay::get_avatar_manager();
		for (roblox::player entity : globals::instances::cachedplayers) {

			if (avatar_mgr) {
				if (!entity.pictureDownloaded) {
					avatar_mgr->requestAvatar(entity.userid.address);
				}
				else {
					continue;
				}

			}
			else {
				break;
			}
		}

		static HWND robloxWindow = FindWindowA(0, "Roblox");
		robloxWindow = FindWindowA(0, "Roblox");
		update_avatars();
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				done = true;
				break;
			}
		}
		if (done == true)
		{
			break;
		}
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_ResizeWidth = g_ResizeHeight = 0;
			CreateRenderTarget();
		}
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		movewindow(hwnd);



		const bool team = (*globals::combat::flags)[0] != 0;
		const bool knock = (*globals::combat::flags)[1] != 0;
		const bool range = (*globals::combat::flags)[2] != 0;
		const bool health = (*globals::combat::flags)[3] != 0;
		const bool walls = (*globals::combat::flags)[4] != 0;
		globals::combat::teamcheck = team;
		globals::combat::knockcheck = knock;
		globals::combat::rangecheck = range;
		globals::combat::healthcheck = health;
		globals::combat::wallcheck = walls;

		if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hwnd)) {
			globals::focused = true;
		}
		else {
			globals::focused = false;
		}
		if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hwnd))
		{
			static bool firsssssssssssssssss = true;
			if (globals::focused && firsssssssssssssssss) {
				overlay::visible = true;
				firsssssssssssssssss = false;
			}
			auto drawbglist = ImGui::GetBackgroundDrawList();
			POINT cursor_pos;
			GetCursorPos(&cursor_pos);
			ScreenToClient(robloxWindow, &cursor_pos);
			ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
			render_keybind_list();
			render_watermark();
			render_target_hud();

			if (overlay::visible)
			{
				static ImVec2 current_dimensions;
				//render_player_list();
				render_explorer();
				current_dimensions = ImVec2(globals::instances::visualengine.GetDimensins().x, globals::instances::visualengine.GetDimensins().y);
				ImGui::GetBackgroundDrawList()->AddRectFilled(
					ImVec2(0, 0),
					current_dimensions,
					ImGui::GetColorU32(ImVec4(0.12, 0.12, 0.12, 0.89f)),
					0
				);
			}
			if (globals::combat::drawfov) {
				if (globals::combat::glowfov) {
					drawbglist->AddShadowCircle(mousepos, globals::combat::fovsize, ColorConvert(globals::combat::fovglowcolor), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 64);
				}
				drawbglist->AddCircle(mousepos, globals::combat::fovsize - 1, IM_COL32(0, 0, 0, 255));
				drawbglist->AddCircle(mousepos, globals::combat::fovsize, ColorConvert(globals::combat::fovcolor));
				drawbglist->AddCircle(mousepos, globals::combat::fovsize + 1, IM_COL32(0, 0, 0, 255));
			}
			if (globals::combat::drawsfov) {
				POINT cursor_pos;
				GetCursorPos(&cursor_pos);
				ScreenToClient(robloxWindow, &cursor_pos);
				ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
				if (globals::combat::glowsfov) {
					drawbglist->AddShadowCircle(mousepos, globals::combat::sfovsize, ColorConvert(globals::combat::sfovglowcolor), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 64);
				}
				drawbglist->AddCircle(mousepos, globals::combat::sfovsize - 1, IM_COL32(0, 0, 0, 255));
				drawbglist->AddCircle(mousepos, globals::combat::sfovsize, ColorConvert(globals::combat::sfovcolor));
				drawbglist->AddCircle(mousepos, globals::combat::sfovsize + 1, IM_COL32(0, 0, 0, 255));
			}
			if (GetAsyncKeyState(VK_RSHIFT) & 1)
			{
				overlay::visible = !overlay::visible;
			}
			if (GetAsyncKeyState(VK_F1) & 1)
			{
				overlay::visible = !overlay::visible;
			}
			if (GetAsyncKeyState(VK_INSERT) & 1)
			{
				overlay::visible = !overlay::visible;
			}
			if (GetAsyncKeyState(VK_HOME) & 1)
			{
				overlay::visible = !overlay::visible;
			}
			ImVec4 original_color = ImVec4(1.f, 0.518f, 0.22f, alpha);
			ImVec4 lighter_color = ImVec4(1.f, 0.855f, 0.757f, alpha);
			ImU32 accent_color_u32 = ImGui::ColorConvertFloat4ToU32(original_color);
			ImU32 border_accent_color_u32 = ImGui::ColorConvertFloat4ToU32(lighter_color);
			if (overlay::visible) {
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
				ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_Always);
				ImGui::Begin("medusa.lol", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
				{
					// Modern black and white theme with sidebar layout
					const char* tabNames[] = { "Aimbot", "Visuals", "Rage", "Lua", "Players", "Settings" };
					const char* tabIcons[] = { "🎯", "👁️", "⚡", "🔧", "👥", "⚙️" };
					static int selected_tab = 0;
					static int selected_subtab = 0;
					
					// Main background
					ImDrawList* draw = ImGui::GetWindowDrawList();
					draw->AddRectFilled(
						ImGui::GetWindowPos(),
						ImGui::GetWindowPos() + ImGui::GetWindowSize(),
						IM_COL32(8, 8, 8, alpha * 255)
					);
					
					// Title with larger font and subtle shadow
					const char* title_text = "medusa.lol";
					ImVec2 text_pos = ImGui::GetCursorScreenPos();
					ImFont* font = ImGui::GetFont();
					float font_size = ImGui::GetFontSize() * 1.5f; // Larger font
					ImVec2 text_size = ImGui::CalcTextSize(title_text);
					
					// Draw title with shadow effect
					draw->AddText(font, font_size, ImVec2(text_pos.x + 1, text_pos.y + 1), IM_COL32(0, 0, 0, 100), title_text);
					draw->AddText(font, font_size, text_pos, IM_COL32(255, 255, 255, 255), title_text);
					
					ImGui::Dummy(text_size);
					ImGui::Spacing();
					
					// Sidebar dimensions
					float sidebar_width = 200.0f;
					float content_width = ImGui::GetWindowWidth() - sidebar_width - 20.0f;
					
					// Draw sidebar background
					ImVec2 sidebar_pos = ImGui::GetWindowPos() + ImVec2(10, 60);
					ImVec2 sidebar_size = ImVec2(sidebar_width, ImGui::GetWindowHeight() - 80);
					
					// Sidebar background with rounded corners
					draw->AddRectFilled(
						sidebar_pos,
						sidebar_pos + sidebar_size,
						IM_COL32(15, 15, 15, alpha * 255),
						12.0f
					);
					
					// Sidebar border
					draw->AddRect(
						sidebar_pos,
						sidebar_pos + sidebar_size,
						IM_COL32(40, 40, 40, alpha * 255),
						12.0f,
						0,
						1.0f
					);
					
					// Sidebar title
					ImGui::SetCursorPos(ImVec2(20, 70));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, alpha));
					ImGui::Text("Navigation");
					ImGui::PopStyleColor();
					ImGui::Spacing();
					
					// Sidebar tab buttons
					ImGui::SetCursorPos(ImVec2(15, 100));
					for (int i = 0; i < IM_ARRAYSIZE(tabNames); ++i) {
						ImGui::PushID(i);
						
						bool is_active = (selected_tab == i);
						bool is_hovered = ImGui::IsMouseHoveringRect(
							ImGui::GetWindowPos() + ImVec2(15, 100 + i * 45),
							ImGui::GetWindowPos() + ImVec2(15 + sidebar_width - 10, 100 + i * 45 + 40)
						);
						
						// Button background
						ImU32 bg_color = is_active ? 
							IM_COL32(70, 130, 180, alpha * 255) : // Blue when active
							(is_hovered ? IM_COL32(30, 30, 30, alpha * 255) : IM_COL32(20, 20, 20, alpha * 255));
						
						draw->AddRectFilled(
							ImGui::GetWindowPos() + ImVec2(15, 100 + i * 45),
							ImGui::GetWindowPos() + ImVec2(15 + sidebar_width - 10, 100 + i * 45 + 40),
							bg_color,
							8.0f
						);
						
						// Button border
						draw->AddRect(
							ImGui::GetWindowPos() + ImVec2(15, 100 + i * 45),
							ImGui::GetWindowPos() + ImVec2(15 + sidebar_width - 10, 100 + i * 45 + 40),
							IM_COL32(50, 50, 50, alpha * 255),
							8.0f,
							0,
							1.0f
						);
						
						// Tab button (invisible but functional)
						ImGui::SetCursorPos(ImVec2(15, 100 + i * 45));
						if (ImGui::InvisibleButton(("##tab" + std::to_string(i)).c_str(), ImVec2(sidebar_width - 10, 40))) {
							selected_tab = i;
							selected_subtab = 0; // Reset subtab when switching tabs
						}
						
						// Tab content (icon + text)
						ImGui::SetCursorPos(ImVec2(25, 110 + i * 45));
						ImGui::Text("%s", tabIcons[i]);
						ImGui::SameLine();
						ImGui::Text("%s", tabNames[i]);
						
						ImGui::PopID();
					}
					
					// Content area styling
					ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 8.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 8.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
					
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, alpha));
					ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, alpha));
					ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, alpha));
					ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.20f, 0.20f, alpha));
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, alpha));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, alpha));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, alpha));
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.25f, 0.25f, alpha));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, alpha));
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.35f, 0.35f, alpha));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, alpha));
					ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.4f, 0.7f, 1.0f, alpha));
					ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.4f, 0.7f, 1.0f, alpha));
					ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.5f, 0.8f, 1.0f, alpha));
					ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.3f, 0.3f, alpha));
					ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, ImVec4(0.4f, 0.4f, 0.4f, alpha));
					ImGui::PushStyleColor(ImGuiCol_SeparatorActive, ImVec4(0.5f, 0.5f, 0.5f, alpha));
					
					// Content area
					ImGui::SetCursorPos(ImVec2(sidebar_width + 20, 60));
					ImGui::BeginChild("ContentArea", ImVec2(content_width, ImGui::GetWindowHeight() - 80), true);
					{
						ImGui::Spacing();
					if (selected_tab == 0) {
						// Aimbot tab content
						ImGui::Text("Aimbot Configuration");
						ImGui::Separator();
						ImGui::Spacing();
						
						// Get window dimensions for this tab
						float window_width = ImGui::GetWindowSize().x;
						float available_height = ImGui::GetContentRegionAvail().y;
						
						// Two-column layout
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
						ImGui::BeginChild("aiming_left", ImVec2(0, 0), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2 - 20, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);
							ImGui::Text("Aim");
							ImAdd::CheckBox("Enable Aimbot", &globals::combat::aimbot);
							ImGui::SameLine(window_width / 2 - 15 - ImGui::CalcTextSize(globals::combat::aimbotkeybind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::combat::aimbotkeybind, ImVec2(30, 10));
							ImAdd::CheckBox("Sticky Aim", &globals::combat::stickyaim);
							ImAdd::Combo("Type", &globals::combat::aimbottype, "Camera\0Mouse\0");
							ImGui::Separator();
							ImAdd::CheckBox("Enable Fov", &globals::combat::usefov);
							ImAdd::CheckBox("Visualize Fov", &globals::combat::drawfov);
							ImGui::SameLine();
							ImAdd::ColorEdit4("##FovColor                                             ", globals::combat::fovcolor);
							ImAdd::CheckBox("Fov Glow", &globals::combat::glowfov);
							ImGui::SameLine();
							ImAdd::ColorEdit4("##FovGlowColor                                                        ", globals::combat::fovglowcolor);
							ImAdd::SliderFloat("Fov Radius", &globals::combat::fovsize, 10, 1000);
							ImGui::Separator();
							ImAdd::CheckBox("Smoothing", &globals::combat::smoothing);
							ImAdd::SliderFloat("Smoothing X", &globals::combat::smoothingx, 1, 50);
							ImAdd::SliderFloat("Smoothing Y", &globals::combat::smoothingy, 1, 50);
							ImGui::Separator();
							ImAdd::CheckBox("Prediction", &globals::combat::predictions);
							ImAdd::SliderFloat("Prediction X", &globals::combat::predictionsx, 1, 15);
							ImAdd::SliderFloat("Prediction Y", &globals::combat::predictionsy, 1, 15);
							ImGui::Separator();
							std::vector<const char*> flags = { "Team", "Knocked", "Range", "Health", "WallCheck", "Grabbed", "Forcefield" };
							if (globals::combat::flags == nullptr) {
								globals::combat::flags = new std::vector<int>(flags.size(), 0);
							}
							ImAdd::MultiCombo("Checks", globals::combat::flags, flags);
							//ImAdd::CheckBox("Auto-Switch On Condition", &globals::combat::autoswitch);
							ImAdd::SliderFloat("Distance", &globals::combat::range, 1, 25);
							ImAdd::SliderFloat("Health", &globals::combat::healththreshhold, 0, 100);
						}
						ImGui::EndChild();
						
						ImGui::NextColumn();
						
						// Right column
						ImGui::BeginChild("aiming_right", ImVec2(0, 0), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.035f, 0.035f, 0.035f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.15f, alpha * 0.4f));

							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);

							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);

							// Subtabs
							ImGui::Text("Combat Options");
							ImGui::Separator();
							ImGui::Spacing();
							
							// Subtab buttons
							ImVec2 button_size = ImVec2((ImGui::GetContentRegionAvail().x / 2.f) - 5, 0);
							ImAdd::TabButton("Triggerbot", &selected_subtab, 0, button_size, 0);
							ImGui::SameLine();
							ImAdd::TabButton("Silent", &selected_subtab, 1, button_size, 0);
							static bool enable_aimbot = false;
							static int aimbot_type = 0;
							const char* aimbot_type_options[] = { "auto", "camera", "mouse" };
							static float max_aimbot_distance = 500.0f;

							switch (selected_subtab) {
							case 0:
								ImGui::Text("Triggerbot");

								ImAdd::CheckBox("TriggerBot", &globals::combat::triggerbot);
								ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::triggerbotkeybind.get_key_name().c_str()).x / 2 - 40, 0);
								Bind(&globals::combat::triggerbotkeybind, ImVec2(40, 10));
								ImAdd::CheckBox("Range                   ", &globals::combat::triggerbotrange);
								ImAdd::SliderFloat("Range        ", &globals::combat::triggerbotrangevalue, 10, 500);
								ImAdd::SliderFloat("Delay M/S", &globals::combat::delay, 0, 50);
								ImAdd::SliderFloat("Release Delay M/S", &globals::combat::releasedelay, 0, 50);
								ImAdd::CheckBox("Knife Check", &globals::visuals::esppreview);

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							case 1:
								ImGui::Text("Silent");

								ImAdd::CheckBox("Enable Silentaim", &globals::combat::silentaim);
								ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::silentaimkeybind.get_key_name().c_str()).x / 2 - 40, 0);
								Bind(&globals::combat::silentaimkeybind, ImVec2(40, 10));
								ImAdd::CheckBox("Sticky Aim  ", &globals::combat::stickyaimsilent);
								ImAdd::SliderInt("Hit Chance", &globals::combat::hitchance, 1, 100);

								ImAdd::CheckBox("Enable Fov ", &globals::combat::usesfov);
								ImAdd::CheckBox("Visualize Fov", &globals::combat::drawsfov);
								ImGui::SameLine(); ImAdd::ColorEdit4("                                             ", globals::combat::sfovcolor);
								ImAdd::CheckBox("Fov Glow ", &globals::combat::glowsfov);
								ImGui::SameLine(); ImAdd::ColorEdit4("                                                       ", globals::combat::sfovglowcolor);
								ImAdd::SliderFloat("Fov Radius ", &globals::combat::sfovsize, 10, 1000);

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							}

							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
						
						ImGui::Columns(1); // Reset columns
					}
					else if (selected_tab == 1) {
						// Visuals tab
						ImGui::Text("Visuals Configuration");
						ImGui::Separator();
						ImGui::Spacing();
						
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
						ImGui::BeginChild("visuals_left", ImVec2(0, 0), true);
						{
							ImGui::Text("ESP Settings");
							ImGui::Separator();
							ImGui::Spacing();
							
							ImAdd::CheckBox("Enable ESP", &globals::visuals::visuals);
							ImAdd::CheckBox("Box ESP", &globals::visuals::boxes);
							ImAdd::CheckBox("Name ESP", &globals::visuals::names);
							ImAdd::CheckBox("Health Bar", &globals::visuals::healthbar);
							ImAdd::CheckBox("Distance", &globals::visuals::distance);
							ImAdd::CheckBox("Tracers", &globals::visuals::tracers);
							ImAdd::CheckBox("Skeleton ESP", &globals::visuals::skeletons);
						}
						ImGui::EndChild();
						
						ImGui::NextColumn();
						
						// Right column
						ImGui::BeginChild("visuals_right", ImVec2(0, 0), true);
						{
							ImGui::Text("Visual Effects");
							ImGui::Separator();
							ImGui::Spacing();
							
							ImAdd::CheckBox("Glow ESP", &globals::visuals::glow);
							ImAdd::SliderFloat("Glow Intensity", &globals::visuals::glowintensity, 0.1f, 2.0f);
							ImAdd::CheckBox("World ESP", &globals::visuals::worldesp);
							ImAdd::CheckBox("Item ESP", &globals::visuals::itemesp);
							ImAdd::CheckBox("Vehicle ESP", &globals::visuals::vehicleesp);
							ImAdd::CheckBox("Optimize Rendering", &globals::visuals::optimize);
							ImAdd::SliderFloat("Max Distance", &globals::visuals::maxdistance, 100.0f, 1000.0f);
						}
						ImGui::EndChild();
						
						ImGui::Columns(1);
					}
					else if (selected_tab == 2) {
						// Rage tab
						ImGui::Text("Rage Configuration");
						ImGui::Separator();
						ImGui::Spacing();
						
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
				
						
					}
					else if (selected_tab == 3) {
						// Lua tab
						ImGui::Text("Lua Scripts");
						ImGui::Separator();
						ImGui::Spacing();
						
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
						ImGui::BeginChild("lua_left", ImVec2(0, 0), true);
						{
							ImGui::Text("Script Management");
							ImGui::Separator();
							ImGui::Spacing();
							
							ImAdd::CheckBox("Enable Lua", &globals::misc::lua_enabled);
							ImAdd::CheckBox("Auto Execute", &globals::misc::auto_execute);
							ImAdd::CheckBox("Lua Debug", &globals::misc::lua_debug);
						}
						ImGui::EndChild();
						
						ImGui::NextColumn();
						
						// Right column
						ImGui::BeginChild("lua_right", ImVec2(0, 0), true);
						{
							ImGui::Text("Performance");
							ImGui::Separator();
							ImGui::Spacing();
							
							ImAdd::CheckBox("Optimize Rendering", &globals::misc::optimize_rendering);
							ImAdd::CheckBox("Reduce CPU Usage", &globals::misc::reduce_cpu);
							ImAdd::SliderFloat("Max FPS", &globals::misc::max_fps, 30.0f, 300.0f);
						}
						ImGui::EndChild();
						
						ImGui::Columns(1);
					}
					else if (selected_tab == 4) {
						// Players tab
						ImGui::Text("Player Management");
						ImGui::Separator();
						ImGui::Spacing();
						
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
						ImGui::BeginChild("players_left", ImVec2(0, 0), true);
						{
							ImGui::Text("Player List");
							ImGui::Separator();
							ImGui::Spacing();
							
							// Player list would go here
							ImGui::Text("Player list functionality");
						}
						ImGui::EndChild();
						
						ImGui::NextColumn();
						
						// Right column
						ImGui::BeginChild("players_right", ImVec2(0, 0), true);
						{
							ImGui::Text("Player Actions");
							ImGui::Separator();
							ImGui::Spacing();
							
							ImGui::Text("Player action controls");
						}
						ImGui::EndChild();
						
						ImGui::Columns(1);
					}
					else if (selected_tab == 5) {
						// Settings tab
						ImGui::Text("Settings Configuration");
						ImGui::Separator();
						ImGui::Spacing();
						
						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, content_width / 2.0f - 10);
						
						// Left column
						ImGui::BeginChild("settings_left", ImVec2(0, 0), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x0f0f0f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x262626, alpha * 0.4f));
							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2 - 23.5, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);
							ImGui::Text("general");
							//static bool enable_aimbot = false;
							if (ImAdd::CheckBox("Watermark", &globals::misc::watermark)) {
							}
							std::vector<const char*> stuff = { "FPS", "Username", "Date" };
							if (globals::misc::watermarkstuff == nullptr) {
								globals::misc::watermarkstuff = new std::vector<int>(stuff.size(), 0);
							}
							ImAdd::MultiCombo("Styling", globals::misc::watermarkstuff, stuff);
							if (ImAdd::CheckBox("VSYNC", &globals::misc::vsync)) {
							}
							ImAdd::CheckBox("Keybind List", &globals::misc::keybinds);
							ImAdd::Combo("Keybind Style", &globals::misc::keybindsstyle, "Dynamic\0Static\0");
							ImAdd::CheckBox("TargetHud", &globals::misc::targethud);

							ImAdd::CheckBox("Streamproof", &globals::misc::streamproof);
							ImAdd::CheckBox("Crosshair", &globals::misc::colors);
							if (ImAdd::Button("Eject", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
								exit(0);
							}
							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("aiming_right_1", ImVec2(window_width / 2 - 8.5f, 0), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.035f, 0.035f, 0.035f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.15f, alpha * 0.4f));
							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);
							ImGui::Text("configuration");
							ImVec2 availableSize = ImGui::GetContentRegionAvail();
							availableSize.y = availableSize.y;
							g_config_system.render_config_ui(window_width / 2 - 12, availableSize.y);
							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
					}
					else if (selected_tab == 4) {
						render_player_list();
					}
					else if (selected_tab == 3) {
						ImGui::BeginGroup();
						{
							static char luaCode[8192] = "-- Write your Lua code here\nprint(\"discord.gg/medusalol\")";
							static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
							ImVec2 availableSize = ImGui::GetContentRegionAvail();
							availableSize.y = availableSize.y;
							float lineNumberWidth = 50.0f;
							float buttonHeight = 25.0f;
							float spacing = 5.0f;
							ImVec2 textSize = ImVec2(availableSize.x - lineNumberWidth - spacing, availableSize.y - buttonHeight - spacing * 2);
							static float currenty;
							ImGui::BeginGroup();
							{
								int lineCount = 1;
								for (int i = 0; luaCode[i] != '\0'; i++) {
									if (luaCode[i] == '\n') lineCount++;
								}
								ImGui::BeginGroup();
								{
									ImGui::BeginChild("LineNumbers", ImVec2(lineNumberWidth, textSize.y), true, ImGuiWindowFlags_NoScrollbar);
									{
										ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
										float lineHeight = ImGui::GetTextLineHeight();
										for (int i = 1; i <= lineCount; i++) {
											ImGui::Text("%3d", i);
											if (i < lineCount) {
												ImGui::SetCursorPosY(ImGui::GetCursorPosY());
											}
										}
										ImGui::PopStyleColor();
									}
									ImGui::EndChild();
									ImGui::SameLine();
									ImGui::BeginChild("CodeEditor", textSize, true);
									{
										ImGui::PushFont(ImGui::GetFont());
										ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
										if (ImGui::InputTextMultiline("##LuaCode", luaCode, sizeof(luaCode),
											ImVec2(-1, -1), flags)) {
										}
										ImGui::PopStyleVar();
										ImGui::PopFont();
									}
									ImGui::EndChild();
								}
								ImGui::EndGroup();
								ImGui::Spacing();
								if (ImGui::Button("Execute", ImVec2(80, buttonHeight))) {
								}
								ImGui::SameLine();
								if (ImGui::Button("Clear", ImVec2(80, buttonHeight))) {
									strcpy_s(luaCode, sizeof(luaCode), "");
								}
								ImGui::SameLine();
								if (ImGui::Button("Load File", ImVec2(80, buttonHeight))) {
								}
								ImGui::SameLine();
								if (ImGui::Button("Save File", ImVec2(80, buttonHeight))) {
								}
								ImGui::SameLine();
								ImGui::Text("Lines: %d", lineCount);
							}
							ImGui::EndGroup();
						}
						ImGui::EndGroup();
					}
					else if (selected_tab == 1) {
						ImVec2 child_pos = ImGui::GetWindowPos();
						ImVec2 child_size = ImGui::GetContentRegionAvail();
						float window_width = ImGui::GetWindowSize().x;
						float available_height = ImGui::GetContentRegionAvail().y;
						float bg_extra_height = 40.0f;
						ImGui::BeginChild("aiming_left_1", ImVec2(window_width / 2 - 15, available_height), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x0f0f0f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x262626, alpha * 0.4f));
							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2 - 23.5, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);
							ImGui::Text("Visuals");
							ImAdd::CheckBox("Enable Visuals", &globals::visuals::visuals);
							ImGui::Separator();
							ImAdd::CheckBox("Boxes", &globals::visuals::boxes);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##boxcolor", globals::visuals::boxcolors);
							if (globals::visuals::boxes) {

								ImAdd::Combo("Box Type", &globals::visuals::boxtype, "Bounding\0Corners\0");

								std::vector<const char*> box_overlays = { "Outline", "Glow", "Fill" };
								ImAdd::MultiCombo("Box Options", globals::visuals::box_overlay_flags, box_overlays);
							}

							ImGui::Separator();

							ImAdd::CheckBox("Enable Name ESP", &globals::visuals::name);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##namecolor", globals::visuals::namecolor);
							ImAdd::Combo("Name Type", &globals::visuals::nametype, "Username\0Display Name\0");

							ImGui::Separator();

							ImAdd::CheckBox("Enable Health Bar", &globals::visuals::healthbar);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##healthcolor1", globals::visuals::healthbarcolor);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48, 0);
							ImAdd::ColorEdit4("##healthcolor2", globals::visuals::healthbarcolor1);


							ImAdd::CheckBox("Enable Tool Name", &globals::visuals::toolesp);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##toolcolor", globals::visuals::toolespcolor);

							ImAdd::CheckBox("Enable Distance", &globals::visuals::distance);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##distancecolor", globals::visuals::distancecolor);

							ImAdd::CheckBox("Enable Skeleton", &globals::visuals::skeletons);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##skeletoncolor", globals::visuals::skeletonscolor);

							ImAdd::CheckBox("Enable Snaplines", &globals::visuals::snapline);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##snaplinecolor", globals::visuals::snaplinecolor);
							ImGui::Separator();
							ImAdd::CheckBox("Enable Chams", &globals::visuals::chams);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##chamscolor", globals::visuals::chams_fillcolor);




							ImGui::Separator();
							ImAdd::CheckBox("Enable OOF Arrows", &globals::visuals::oofarrows);
							ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
							ImAdd::ColorEdit4("##oofcolor", globals::visuals::oofcolor);
							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("aiming_right_1", ImVec2(window_width / 2 - 8.5f, available_height), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.035f, 0.035f, 0.035f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.15f, alpha * 0.4f));

							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);

							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);

							// subtabs
							ImVec2 button_size = ImVec2((ImGui::GetWindowWidth() / 2.f), 0);

							ImGui::SetCursorPos(ImVec2(0, 3));
							ImAdd::TabButton("Esp Options", &selected_subtab, 0, button_size, 0);
							ImGui::SameLine();
							ImGui::SetCursorPosX(button_size.x - 2);
							ImAdd::TabButton("World Effects", &selected_subtab, 1, button_size, 0);
							static bool enable_aimbot = false;
							static int aimbot_type = 0;
							const char* aimbot_type_options[] = { "auto", "camera", "mouse" };
							static float max_aimbot_distance = 500.0f;

							switch (selected_subtab) {
							case 0:

								ImGui::Text("Box Options");
								ImGui::Separator();
								ImGui::Spacing();
								ImGui::Spacing();

								ImAdd::ColorEdit4("Fill Color", globals::visuals::boxfillcolor);

								ImGui::Spacing();
								ImGui::Spacing();

								ImAdd::ColorEdit4("Glow Color", globals::visuals::glowcolor);

								ImGui::Spacing();
								ImGui::Spacing();

								ImGui::Text("Health Bar Options");
								ImGui::Separator();
								ImAdd::CheckBox("Heathbar Outline", &globals::visuals::h_outline);
								ImAdd::CheckBox("Heathbar Gradient", &globals::visuals::h_gradient);
								ImAdd::CheckBox("Heathbar Glow", &globals::visuals::h_glow);
								ImGui::Text("Chams Options");
								ImGui::Separator();
								static bool stltltlt;
								ImAdd::CheckBox("Chams Wireframe (Coming Soon)", &globals::visuals::esppreview);
								ImAdd::CheckBox("Chams Outline", &globals::visuals::chams_outline_s);
								ImAdd::CheckBox("Chams Glow", &globals::visuals::chams_glow_s);
								ImAdd::CheckBox("Chams Fill", &globals::visuals::chams_highlight_s);

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							case 1:
								ImGui::Text("World Effects");

								ImGui::Spacing();
								ImGui::Text("Not Done Since Im Tryna Rush Release");

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							}

							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
					}
					else if (selected_tab == 2) {
						ImVec2 child_pos = ImGui::GetWindowPos();
						ImVec2 child_size = ImGui::GetContentRegionAvail();
						float window_width = ImGui::GetWindowSize().x;
						float available_height = ImGui::GetContentRegionAvail().y;
						float bg_extra_height = 40.0f;
						ImGui::BeginChild("aiming_left_1", ImVec2(window_width / 2 - 15, available_height), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x0f0f0f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(HexToColorVec4(0x262626, alpha * 0.4f));
							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2 - 23.5, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);
							ImGui::Text("Rage");
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
							ImAdd::CheckBox("Speed ", &globals::misc::speed);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::speedkeybind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::speedkeybind, ImVec2(40, 10));
							ImAdd::Combo("Mode", &globals::misc::speedtype, "WalkSpeed\0Velocity\0Position\0");
							ImAdd::SliderFloat("Speed", &globals::misc::speedvalue, 1, 750);
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImGui::Separator();
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImAdd::CheckBox("JumpPower", &globals::misc::jumppower);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::jumppowerkeybind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::jumppowerkeybind, ImVec2(40, 10));
							ImAdd::SliderFloat("Power", &globals::misc::jumpowervalue, 1, 750);
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImGui::Separator();
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImAdd::CheckBox("Flight", &globals::misc::flight);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::flightkeybind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::flightkeybind, ImVec2(40, 10));
							ImAdd::Combo("Mode ", &globals::misc::flighttype, "Position\0Velocity\0");
							ImAdd::SliderFloat("Speed ", &globals::misc::flightvalue, 1, 750);
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImGui::Separator();
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImAdd::CheckBox("VoidHide", &globals::misc::voidhide);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::voidhidebind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::voidhidebind, ImVec2(40, 10));
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImGui::Separator();
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImAdd::CheckBox("AutoStomp", &globals::misc::autostomp);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::stompkeybind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::stompkeybind, ImVec2(40, 10));
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImGui::Separator();
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
							ImAdd::CheckBox("Freecam", &globals::misc::spectate);
							ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::spectatebind.get_key_name().c_str()).x / 2 - 40, 0);
							Bind(&globals::misc::spectatebind, ImVec2(40, 10));
							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("aiming_right_1", ImVec2(window_width / 2 - 8.5f, available_height), true);
						{
							ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
							ImDrawList* draw_list = ImGui::GetWindowDrawList();
							ImU32 color_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.035f, 0.035f, 0.035f, alpha * 0.478f));
							ImU32 color_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.15f, alpha * 0.4f));

							draw_list->AddRect(ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255),
								0.0F,
								0,
								1.25f
							);
							draw_list->AddRectFilled(
								ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);
							draw_list->AddRectFilledMultiColor(ImGui::GetWindowPos(),
								ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - (available_height / 1.5f)),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(25, 25, 25, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f),
								IM_COL32(15, 15, 15, alpha * 255.f)
							);

							ImVec2 line_start = ImVec2(cursor_pos.x - 10.f, cursor_pos.y - 7.0f);
							ImVec2 line_end = ImVec2(cursor_pos.x + window_width / 2, cursor_pos.y - 7.0f);
							draw_list->AddLine(line_start, line_end, accent_color_u32, 2.0f);

							// subtabs
							ImVec2 button_size = ImVec2((ImGui::GetWindowWidth() / 2.f), 0);

							ImGui::SetCursorPos(ImVec2(0, 3));
							ImAdd::TabButton("Dahood", &selected_subtab, 0, button_size, 0);
							ImGui::SameLine();
							ImGui::SetCursorPosX(button_size.x - 2);
							ImAdd::TabButton("Other", &selected_subtab, 1, button_size, 0);
							static bool enable_aimbot = false;
							static int aimbot_type = 0;
							const char* aimbot_type_options[] = { "auto", "camera", "mouse" };
							static float max_aimbot_distance = 500.0f;

							switch (selected_subtab) {
							case 0:
								ImGui::Text("Dahood");

								ImAdd::CheckBox("RapidFire (experimental)", &globals::misc::rapidfire);
								ImAdd::CheckBox("Antistomp", &globals::misc::antistomp);
								ImAdd::CheckBox("AutoReload", &globals::misc::autoreload);
								ImAdd::CheckBox("AutoArmor", &globals::misc::autoarmor);
								ImAdd::CheckBox("Vehicle-Fly", &globals::misc::bikefly);
								ImAdd::CheckBox("Anti Aim (Experimental)", &globals::combat::antiaim);

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							case 1:
								ImGui::Text("Other");

								ImGui::Spacing();
								ImGui::Text("Idk Yet???");

								ImGui::GetForegroundDrawList()->AddRect(
									ImGui::GetWindowPos(),
									ImGui::GetWindowPos() + ImGui::GetWindowSize(),
									IM_COL32(0, 0, 0, alpha * 255)
								);
								break;
							}

							ImGui::GetForegroundDrawList()->AddRect(
								ImGui::GetWindowPos(),
								ImGui::GetWindowPos() + ImGui::GetWindowSize(),
								IM_COL32(0, 0, 0, alpha * 255)
							);
						}
						ImGui::EndChild();
					}
					ImGui::GetForegroundDrawList()->AddRect(
						ImGui::GetWindowPos(),
						ImGui::GetWindowPos() + ImGui::GetWindowSize(),
						IM_COL32(0, 0, 0, alpha * 255)
					);
				}
				ImGui::End();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
				// ImGui::PopFontShadow();
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			overlay::visible = false;
		}
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::Begin("esp", NULL,
			ImGuiWindowFlags_NoBackground
			|
			ImGuiWindowFlags_NoResize
			|
			ImGuiWindowFlags_NoCollapse
			|
			ImGuiWindowFlags_NoTitleBar
			|
			ImGuiWindowFlags_NoInputs
			|
			ImGuiWindowFlags_NoMouseInputs
			|
			ImGuiWindowFlags_NoDecoration
			|
			ImGuiWindowFlags_NoMove); {
			globals::instances::draw = ImGui::GetBackgroundDrawList();
			visuals::run();
		}
		ImGui::End();
		if (overlay::visible) {
			SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
		}
		else
		{
			SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
		}
		if (globals::misc::streamproof)
		{
			SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
		}
		else
		{
			SetWindowDisplayAffinity(hwnd, WDA_NONE);
		}
		// // Notifications::Update();
		// // Notifications::Render();

		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		if (globals::misc::vsync) {
			g_pSwapChain->Present(1, 0);
		}
		else {
			g_pSwapChain->Present(0, 0);
		}
		static bool test_sent = false;
		if (!test_sent && overlay::visible) {
			//// Notifications::Success("Overlay Loaded!");
			test_sent = true;
		}
		static float cachedhealth = 0;
		static std::string lastname = "";

		if ((globals::combat::aimbot && globals::combat::aimbotkeybind.enabled)
			|| (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled)) {
			if (globals::instances::cachedtarget.head.address != 0) {
				if (lastname == globals::instances::cachedtarget.name) {
					if (globals::instances::cachedtarget.humanoid.read_health() < cachedhealth) {
						std::string sdfsdf = std::to_string(static_cast<int>(globals::instances::cachedtarget.humanoid.read_health()));
						std::string temph = globals::instances::cachedtarget.name + " HP: " + sdfsdf;
						// // Notifications::Success(temph);
						cachedhealth = globals::instances::cachedtarget.humanoid.read_health();
					}
				}
				else {
					cachedhealth = globals::instances::cachedtarget.humanoid.read_health();
					lastname = globals::instances::cachedtarget.name;
				}
			}
		}

		/* static LARGE_INTEGER frequency;
		static LARGE_INTEGER lastTime;
		static bool timeInitialized = false;
		if (!timeInitialized) {
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&lastTime);
		timeBeginPeriod(1);
		timeInitialized = true;
		}
		const double targetFrameTime = 1.0 / 165;
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
		if (elapsedSeconds < targetFrameTime) {
		DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
		if (sleepMilliseconds > 0) {
		Sleep(sleepMilliseconds);
		}
		}
		do {
		QueryPerformanceCounter(&currentTime);
		elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
		} while (elapsedSeconds < targetFrameTime);
		lastTime = currentTime;*/
	}
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	cleanup_avatar_system();
	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}
bool fullsc(HWND windowHandle)
{
	MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
	if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
	{
		RECT rect;
		if (GetWindowRect(windowHandle, &rect))
		{
			return rect.left == monitorInfo.rcMonitor.left
				&& rect.right == monitorInfo.rcMonitor.right
				&& rect.top == monitorInfo.rcMonitor.top
				&& rect.bottom == monitorInfo.rcMonitor.bottom;
		}
	}
}
void movewindow(HWND hw) {
	HWND target = FindWindowA(0, "Roblox");
	HWND foregroundWindow = GetForegroundWindow();
	if (target != foregroundWindow && hw != foregroundWindow)
	{
		MoveWindow(hw, 0, 0, 0, 0, true);
	}
	else
	{
		RECT rect;
		GetWindowRect(target, &rect);
		int rsize_x = rect.right - rect.left;
		int rsize_y = rect.bottom - rect.top;
		if (fullsc(target))
		{
			rsize_x += 16;
			rect.right -= 16;
		}
		else
		{
			rsize_y -= 39;
			rect.left += 8;
			rect.top += 31;
			rect.right -= 16;
		}
		rsize_x -= 16;
		MoveWindow(hw, rect.left, rect.top, rsize_x, rsize_y, TRUE);
	}
}
bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED)
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;
	CreateRenderTarget();
	return true;
}
void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}
void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;
	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam);
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}