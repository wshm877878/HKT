#include "Panel.h"

#include "Eat.h"
#include "Execute.h"
#include "Grapple.h"
#include "Jujutsu.h"
#include "Module.h"
#include "Needs.h"
#include "Potion.h"
#include "Prompt.h"
#include "Settings.h"
#include "Surrender.h"
#include "WeaponSwap.h"

#include <set>

// The vendored header mixes struct/class and enum types; its warnings are upstream's.
#pragma warning(push)
#pragma warning(disable: 4099 5054)
#include "SKSEMenuFramework/SKSEMenuFramework.h"
#pragma warning(pop)

namespace CIGAR::Panel
{
	namespace
	{
		constexpr auto kSection = "CIGAR";

#ifdef CIGAR_RELEASE
		constexpr bool kRelease = true;
#else
		constexpr bool kRelease = false;
#endif

		struct Label
		{
			std::string_view module;
			const char* title;
			const char* needs;
			const char* what;
		};

		constexpr std::array kLabels{
			Label{ "Bathe", "沐浴", "Bathing in Skyrim - Renewed",
				"进入水中时弹出沐浴提示，在瀑布下弹出淋浴提示。清洗后可以清除污垢。" },
			Label{ "Dress", "脱衣·穿衣", "",
				"在床、衣柜前或水中时弹出脱衣提示。脱下的衣物会被记录，之后可通过穿衣提示一键重新穿上。" },
			Label{ "BaboKey", "绑架行为选择", "BaboDialogue",
				"在被绑架的房间中弹出行为选择提示。无需快捷键，直接通过屏幕提示选择。" },
			Label{ "LockOn", "锁定", "True Directional Movement",
				"战斗中弹出锁定敌人的提示。如果已经处于锁定状态则不会显示。" },
			Label{ "Grapple", "擒拿", "Grapple (Patreon)",
				"战斗中接近敌人时弹出擒拿提示。若在锁定状态下使用，擒拿结束后会自动重新锁定。" },
			Label{ "Deflate", "排出", "Fill Her Up",
				"身体被填满时弹出排出提示。长按按键进行排出。" },
			Label{ "Surrender", "投降", "Acheron (Yamete Kudasai)",
				"战斗中生命值低于40%时弹出投降提示。长按按键投降，长按期间画面会进入子弹时间。" },
			Label{ "Eat", "进食", "Survival Mode (SMI, Gourmet)",
				"处于饥饿状态时弹出吃掉身上最便宜食物的提示。不会选择生肉、酒精或变质食物。" },
			Label{ "WeaponSwap", "武器切换", "",
				"敌人距离较远或逃跑时提示切换为远程武器，靠近时提示切换为近战武器。使用后可切回原武器。" },
			Label{ "Execute", "处决", "Valhalla Combat",
				"敌人失衡（破防）时弹出处决提示。仅在能够真正触发处决动作时才会显示。" },
			Label{ "Jujutsu", "柔术", "",
				"对处于防御状态的人形敌人弹出柔术提示。不会击杀敌人，而是将其击倒并破除防御。" },
			Label{ "Needs", "排泄", "Private Needs - Orgasm",
				"急需如厕时弹出排泄提示。长按按键，找个隐蔽角落解决生理需求。" },
			Label{ "Potion", "喝药", "",
				"生命值低于一半时弹出喝生命药水提示，魔法过半弹出喝法力药水。长按可喝解毒药剂。" },
		};

		const Label* Find(std::string_view a_module)
		{
			for (const auto& label : kLabels) {
				if (label.module == a_module) {
					return &label;
				}
			}
			return nullptr;
		}

		const ImVec4 kDim{ 0.62f, 0.62f, 0.62f, 1.0f };
		const ImVec4 kWarn{ 1.0f, 0.72f, 0.28f, 1.0f };

		struct KeyName
		{
			std::uint32_t code;
			const char* name;
		};

		constexpr std::array kKeys{
			KeyName{ 0x02, "1" }, KeyName{ 0x03, "2" }, KeyName{ 0x04, "3" }, KeyName{ 0x05, "4" },
			KeyName{ 0x06, "5" }, KeyName{ 0x07, "6" }, KeyName{ 0x08, "7" }, KeyName{ 0x09, "8" },
			KeyName{ 0x0A, "9" }, KeyName{ 0x0B, "0" }, KeyName{ 0x0C, "-" }, KeyName{ 0x0D, "=" },
			KeyName{ 0x10, "Q" }, KeyName{ 0x11, "W" }, KeyName{ 0x12, "E" }, KeyName{ 0x13, "R" },
			KeyName{ 0x14, "T" }, KeyName{ 0x15, "Y" }, KeyName{ 0x16, "U" }, KeyName{ 0x17, "I" },
			KeyName{ 0x18, "O" }, KeyName{ 0x19, "P" }, KeyName{ 0x1E, "A" }, KeyName{ 0x1F, "S" },
			KeyName{ 0x20, "D" }, KeyName{ 0x21, "F" }, KeyName{ 0x22, "G" }, KeyName{ 0x23, "H" },
			KeyName{ 0x24, "J" }, KeyName{ 0x25, "K" }, KeyName{ 0x26, "L" }, KeyName{ 0x2C, "Z" },
			KeyName{ 0x2D, "X" }, KeyName{ 0x2E, "C" }, KeyName{ 0x2F, "V" }, KeyName{ 0x30, "B" },
			KeyName{ 0x31, "N" }, KeyName{ 0x32, "M" }, KeyName{ 0x29, "`" }, KeyName{ 0x0F, "Tab" },
			KeyName{ 0x3A, "Caps Lock" }, KeyName{ 0x3B, "F1" }, KeyName{ 0x3C, "F2" }, KeyName{ 0x3D, "F3" },
			KeyName{ 0x3E, "F4" }, KeyName{ 0x3F, "F5" }, KeyName{ 0x40, "F6" }, KeyName{ 0x41, "F7" },
			KeyName{ 0x42, "F8" }, KeyName{ 0x43, "F9" }, KeyName{ 0x44, "F10" }, KeyName{ 0x57, "F11" },
			KeyName{ 0x58, "F12" }, KeyName{ 0x52, "Num 0" }, KeyName{ 0x4F, "Num 1" }, KeyName{ 0x50, "Num 2" },
			KeyName{ 0x51, "Num 3" }, KeyName{ 0x4B, "Num 4" }, KeyName{ 0x4C, "Num 5" }, KeyName{ 0x4D, "Num 6" },
			KeyName{ 0x47, "Num 7" }, KeyName{ 0x48, "Num 8" }, KeyName{ 0x49, "Num 9" },
		};

		std::string NameOf(std::int64_t a_code)
		{
			for (const auto& key : kKeys) {
				if (key.code == a_code) {
					return key.name;
				}
			}
			switch (a_code) {
			case 0x64:
				return "F13";
			case 0x65:
				return "F14";
			case 0x66:
				return "F15";
			case 0x9C:
				return "Num Enter";
			default:
				break;
			}
			if (a_code >= 256 && a_code < 264) {
				return std::format("鼠标 {}", a_code - 255);
			}
			return std::format("#{}", a_code);
		}

		void RenderPromptOnlyItem(std::string_view a_target, const char* a_label, const char* a_hidden, void (*a_apply)())
		{
			bool on = Settings::PromptOnly(a_target);
			if (ImGui::Checkbox(a_label, &on)) {
				Settings::SetPromptOnly(a_target, on);
				SKSE::GetTaskInterface()->AddTask(a_apply);
			}
			ImGui::Indent();
			const auto manual = Settings::ManualKey(a_target);
			const auto manualName = manual >= 0 ? NameOf(manual) : std::string("无记录");
			if (on) {
				ImGui::TextColored(kDim, "模组快捷键已移至 %s(隐藏按键)。原快捷键 %s 目前为空", a_hidden, manualName.c_str());
			} else {
				ImGui::TextColored(kDim, "使用模组自身快捷键。关闭时恢复的快捷键: %s", manualName.c_str());
			}
			ImGui::Unindent();
		}

		void RenderPromptOnly()
		{
			RenderPromptOnlyItem("grapple", "擒拿: 仅提示触发##po-grapple", "F13",
				[] { Grapple::GetSingleton()->CheckKeys(); });
			RenderPromptOnlyItem("surrender", "Acheron 投降: 仅提示触发##po-surrender", "F14",
				[] { Surrender::GetSingleton()->ApplyKeyMode(); });
			RenderPromptOnlyItem("valhalla", "Valhalla 处决: 仅提示触发##po-valhalla", "F15",
				[] { Execute::GetSingleton()->CheckKey(); });
			{
				bool on = Settings::PromptOnly("privateneeds");
				if (ImGui::Checkbox("Private Needs: 仅提示触发##po-privateneeds", &on)) {
					Settings::SetPromptOnly("privateneeds", on);
					SKSE::GetTaskInterface()->AddTask([] { Needs::GetSingleton()->ApplyKeyMode(); });
				}
				ImGui::Indent();
				const auto keys = Needs::GetSingleton()->KeySummary();
				if (on) {
					ImGui::TextColored(kDim, "已解除 PNO 的 6 个快捷键（包括菜单 Y、查看状态 U）。关闭 MCM 时将重新检查");
				} else {
					ImGui::TextColored(kDim, "使用 PNO 自带快捷键。当前按键码: %s", keys.empty() ? "无" : keys.c_str());
				}
				ImGui::Unindent();
			}
			if (ImGui::Button("重新检测模组快捷键")) {
				SKSE::GetTaskInterface()->AddTask([] {
					Grapple::GetSingleton()->CheckKeys();
					Surrender::GetSingleton()->CheckKey();
					Execute::GetSingleton()->CheckKey();
					Needs::GetSingleton()->ApplyKeyMode();
				});
			}
			const auto grapple = Grapple::GetSingleton()->Key();
			const auto surrender = Surrender::GetSingleton()->SurrenderKey();
			const auto execution = Execute::GetSingleton()->ExecutionKey();
			ImGui::TextColored(kDim, "当前: 擒拿 %s, Acheron 投降 %s, Valhalla 处决 %s",
				grapple >= 0 ? NameOf(grapple).c_str() : "无", surrender >= 0 ? NameOf(surrender).c_str() : "无",
				execution >= 0 ? NameOf(execution).c_str() : "无");
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim, "按键仅在读取存档以及点击此按钮时检测。在对应模组 MCM 中修改按键后请点击此按钮。若开启仅提示触发，会自动记录修改后的按键并将其重定向至隐藏键");
			ImGui::PopTextWrapPos();
		}

		void RenderKeys()
		{
			static const auto names = [] {
				std::array<const char*, kKeys.size()> result{};
				for (std::size_t i = 0; i < kKeys.size(); ++i) {
					result[i] = kKeys[i].name;
				}
				return result;
			}();

			const auto keys = Settings::PromptKeys();
			for (std::size_t slot = 0; slot < keys.size(); ++slot) {
				int current = -1;
				for (std::size_t i = 0; i < kKeys.size(); ++i) {
					if (kKeys[i].code == keys[slot]) {
						current = static_cast<int>(i);
					}
				}
				const auto label = std::format("第 {} 个提示按键##key{}", slot + 1, slot);
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::Combo(label.c_str(), &current, names.data(), static_cast<int>(names.size()), 12) && current >= 0) {
					Settings::SetPromptKey(slot, kKeys[current].code);
				}
				ImGui::SameLine();
				ImGui::TextColored(kDim, "(配置文件值 %s)", NameOf(keys[slot]).c_str());
			}
			ImGui::Spacing();
			if (ImGui::Button("恢复默认 (1, 2, 3, 4)")) {
				for (std::size_t slot = 0; slot < keys.size(); ++slot) {
					if (keys[slot] != Settings::kDefaultPromptKeys[slot]) {
						Settings::SetPromptKey(slot, Settings::kDefaultPromptKeys[slot]);
					}
				}
			}
			ImGui::TextColored(kDim, "按屏幕提示出现的先后顺序从第 1 个开始分配。手柄使用 SkyPrompt 默认键位");

			for (std::size_t a = 0; a < keys.size(); ++a) {
				for (std::size_t b = a + 1; b < keys.size(); ++b) {
					if (keys[a] == keys[b]) {
						ImGui::TextColored(kWarn, "警告: 第 %zu 个与第 %zu 个按键重复 (%s)", a + 1, b + 1, NameOf(keys[a]).c_str());
					}
				}
			}
			const std::pair<const char*, std::int64_t> targets[]{
				{ "擒拿", Grapple::GetSingleton()->Key() },
				{ "Acheron 投降", Surrender::GetSingleton()->SurrenderKey() },
				{ "Valhalla 处决", Execute::GetSingleton()->ExecutionKey() },
			};
			for (const auto& [who, code] : targets) {
				for (std::size_t slot = 0; slot < keys.size(); ++slot) {
					if (code >= 0 && static_cast<std::uint32_t>(code) == keys[slot]) {
						ImGui::TextColored(kWarn, "警告: 第 %zu 个按键(%s) 与 %s 快捷键相同", slot + 1, NameOf(code).c_str(), who);
					}
				}
			}
		}

		void RenderModule(const Module* a_module)
		{
			const std::string_view name = a_module->Name();
			const auto* label = Find(name);
			const std::string id = kRelease
			                           ? std::format("{}##{}", label ? label->title : a_module->Name(), name)
			                           : std::format("{} ({})##{}", label ? label->title : a_module->Name(), name, name);

			bool on = Settings::Enabled(name);
			if (ImGui::Checkbox(id.c_str(), &on)) {
				Settings::SetEnabled(name, on);
			}

			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);

			if (label && label->what[0] != '\0') {
				ImGui::TextColored(kDim, "%s", label->what);
			}

			if (label && label->needs[0] != '\0') {
				ImGui::TextColored(kDim, "依赖: %s (未检测到时待机)", label->needs);
			}

			if (!on) {
				ImGui::TextColored(kDim, "已关闭。不显示提示");
			} else if constexpr (!kRelease) {
				const auto gate = a_module->ShownGate();
				const auto line = a_module->ShownLine();
				ImGui::TextColored(kDim, "条件: %s", gate.empty() ? "无记录" : gate.c_str());
				ImGui::TextColored(kDim, "最近: %s", line.empty() ? "无记录" : line.c_str());
			}

			ImGui::PopTextWrapPos();
			ImGui::Unindent();
			ImGui::Spacing();
		}

		void LogFirstDraw(const char* a_page)
		{
			static std::mutex lock;
			static std::set<std::string, std::less<>> drawn;
			std::scoped_lock guard(lock);
			if (drawn.emplace(a_page).second) {
				logs::info("control panel: page {} drawn for the first time", a_page);
			}
		}

		constexpr auto kPageModules = "1. 模块";
		constexpr auto kPageKeys = "2. 快捷键";
		constexpr auto kPageOptions = "3. 详细设置";

		void __stdcall RenderModules()
		{
			LogFirstDraw(kPageModules);
			ImGui::SeparatorText("模块");
			for (const auto* module : Modules()) {
				RenderModule(module);
			}

			ImGui::SeparatorText("状态");
			ImGui::Text("SkyPrompt: %s", Prompts::Available() ? "已连接" : "未找到 (提示功能已禁用)");
			if constexpr (!kRelease) {
				const auto source = Settings::SourceDescription();
				ImGui::TextColored(kDim, "配置文件: %s", source.c_str());
			}
			ImGui::TextColored(kDim, "如果出现问题，请附带 SKSE\\CIGAR.log 日志反馈");
		}

		void __stdcall RenderKeyPage()
		{
			LogFirstDraw(kPageKeys);
			ImGui::SeparatorText("提示按键");
			RenderKeys();

			ImGui::SeparatorText("拦截模组快捷键");
			RenderPromptOnly();
		}

		void __stdcall RenderOptions()
		{
			LogFirstDraw(kPageOptions);

			ImGui::SeparatorText("进食");
			int stage = Settings::EatMinStage();
			if (ImGui::SliderInt("弹出进食提示的饥饿阶段", &stage, Eat::kMinStageLow, Eat::kMinStageHigh)) {
				Settings::SetEatMinStage(stage);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 3 阶段。非战斗中达到此饥饿阶段时弹出最便宜食物的进食提示");

			ImGui::SeparatorText("排泄");
			int needs = Settings::NeedsMinPercent();
			if (ImGui::SliderInt("弹出排泄提示的蓄积百分比", &needs, Needs::kMinPercentLow, Needs::kMinPercentHigh, "%d%%")) {
				Settings::SetNeedsMinPercent(needs);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 50%%。Private Needs 膀胱/肠道蓄积度达到该数值时弹出排泄提示");

			ImGui::SeparatorText("药水");
			{
				auto tune = Settings::PotionTune();
				bool changed = false;
				changed |= ImGui::Checkbox("生命##pot-hp", &tune.health);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("耐力##pot-sp", &tune.stamina);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("法力##pot-mp", &tune.magicka);
				changed |= ImGui::Checkbox("解毒##pot-poison", &tune.curePoison);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("祛病##pot-disease", &tune.cureDisease);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("水下呼吸##pot-water", &tune.waterBreathing);
				if (changed) {
					Settings::SetPotionTune(tune);
					Settings::Save();
				}

				const auto bar = [&tune](const char* a_label, float& a_value, const char* a_help) {
					float percent = a_value * 100.0f;
					const float low = Potion::kThresholdLow * 100.0f;
					const float high = Potion::kThresholdHigh * 100.0f;
					if (ImGui::SliderFloat(a_label, &percent, low, high, "%.0f%%")) {
						a_value = percent / 100.0f;
						Settings::SetPotionTune(tune);
					}
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						Settings::Save();
					}
					ImGui::TextColored(kDim, "%s", a_help);
				};
				bar("生命提示阈值##pot-hp-th", tune.healthThreshold, "默认 50%。生命值低于此比例时提示喝生命药水");
				bar("生命危机阈值##pot-hp-urgent", tune.urgentHealthThreshold, "默认 20%。低于此比例时优先选择强效生命药水");
				bar("耐力提示阈值##pot-sp-th", tune.staminaThreshold, "默认 50%");
				bar("法力提示阈值##pot-mp-th", tune.magickaThreshold, "默认 50%");

				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextColored(kDim, "药水通过效果（恢复数值、解毒/祛病类型）进行判定。包含任何有害效果的药水将被排除。一次仅显示一个提示，优先级为：生命、水下呼吸、耐力、法力、解毒、祛病");
				ImGui::PopTextWrapPos();
			}

			ImGui::SeparatorText("武器切换");
			float swap = Settings::WeaponSwapRange();
			if (ImGui::SliderFloat("切换判定距离", &swap, WeaponSwap::kRangeLow, WeaponSwap::kRangeHigh, "%.0f")) {
				Settings::SetWeaponSwapRange(swap);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 800。敌人在此距离外或逃跑时提示远程武器，在此距离内提示近战武器");

			ImGui::SeparatorText("柔术");
			float reach = Settings::JujutsuReach();
			if (ImGui::SliderFloat("柔术有效距离", &reach, Jujutsu::kReachLow, Jujutsu::kReachHigh, "%.0f")) {
				Settings::SetJujutsuReach(reach);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 250。格挡中的人形敌人在该距离内时弹出柔术提示");

			auto tune = Settings::JujutsuTune();
			float guardPct = tune.guardStun * 100.0f;
			if (ImGui::SliderFloat("失衡条伤害", &guardPct, 0.0f, 100.0f, "%.0f%%")) {
				tune.guardStun = guardPct / 100.0f;
				Settings::SetJujutsuTune(tune);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 15%%。对格挡目标的失衡槽造成的伤害百分比。布娃娃状态另计");

			ImGui::SeparatorText("脱衣·穿衣");
			float range = Settings::PlaceRange();
			if (ImGui::SliderFloat("家具交互距离", &range, Settings::kPlaceRangeMin, Settings::kPlaceRangeMax, "%.0f")) {
				Settings::SetPlaceRange(range);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 250。离开瞄准的床或衣柜超过此距离时取消提示");
		}
	}

	void Register()
	{
		const auto framework = SKSEMenuFramework_Module();
		if (!framework) {
			logs::info("control panel: SKSE Menu Framework is not loaded; no panel");
			return;
		}

		if (!GetProcAddress(framework, "AddSectionItem") || !GetProcAddress(framework, "igCheckbox")) {
			logs::error("control panel: this SKSE Menu Framework lacks AddSectionItem/igCheckbox; no panel");
			return;
		}

		SKSEMenuFramework::SetSection(kSection);
		SKSEMenuFramework::AddSectionItem(kPageModules, RenderModules);
		SKSEMenuFramework::AddSectionItem(kPageKeys, RenderKeyPage);
		SKSEMenuFramework::AddSectionItem(kPageOptions, RenderOptions);
		logs::info("control panel: registered {}/{{{}, {}, {}}} in SKSE Menu Framework", kSection, kPageModules, kPageKeys, kPageOptions);
	}
}
