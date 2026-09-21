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

		// A release build hides what only a mod author reads: the internal module name, the live
		// gate string and the last log line. It shows what the module DOES instead.
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
			// What the player gets from it, in one or two sentences.
			const char* what;
		};

		// Modules missing here still get a switch, titled with their own name.
		constexpr std::array kLabels{
			Label{ "Bathe", "沐浴", "Bathing in Skyrim - Renewed",
				"进入水中时会弹出沐浴提示，在瀑布下会弹出淋浴提示。清洗后可以清除污垢。" },
			Label{ "Dress", "脱衣·穿衣", "",
				"在床、衣柜前或水中时会弹出脱衣提示。脱下的衣物会被记录，之后可通过穿衣提示一键重新穿上。" },
			Label{ "BaboKey", "绑架行为选择", "BaboDialogue",
				"在被绑架的房间中会弹出行为选择提示。无需快捷键，直接通过屏幕提示选择。" },
			Label{ "LockOn", "锁定", "True Directional Movement",
				"战斗中会弹出锁定敌人的提示。如果已经处于锁定状态则不会显示。" },
			Label{ "Grapple", "擒拿", "Grapple (Patreon)",
				"战斗中接近敌人时会弹出擒拿提示。若在锁定状态下使用，擒拿结束后会自动重新锁定。" },
			Label{ "Deflate", "排出", "Fill Her Up",
				"身体被填满时会弹出排出提示。长按按键进行排出。" },
			Label{ "Surrender", "投降", "Acheron (Yamete Kudasai)",
				"战斗中生命值低于40%时会弹出投降提示。长按按键投降，长按期间画面会进入子弹时间。" },
			Label{ "Eat", "进食", "Survival Mode (SMI, Gourmet)",
				"处于饥饿状态时会弹出吃掉身上最便宜食物的提示。不会选择生肉、酒精或变质食物。" },
			Label{ "WeaponSwap", "武器切换", "",
				"敌人距离较远或逃跑时提示切换为远程武器，靠近时提示切换为近战武器。使用后可切回原武器。" },
			Label{ "Execute", "处决", "Valhalla Combat",
				"敌人失衡（破防）时会弹出处决提示。仅在能够真正触发处决动作时才会显示。" },
			Label{ "Jujutsu", "柔术", "",
				"对处于防御状态的人形敌人会弹出柔术提示。不会击杀敌人，而是将其击倒并破除防御。" },
			Label{ "Needs", "排泄", "Private Needs - Orgasm",
				"急需如厕时会弹出排泄提示。长按按键，找个隐蔽角落解决生理需求。" },
			Label{ "Drink", "喝药", "Streamlined Interactions",
				"生命值低于一半时弹出喝生命药水提示，魔法过半弹出喝法力药水。长按可喝解毒药剂。" },
		};

		const Label* FindLabel(std::string_view a_name)
		{
			for (const auto& label : kLabels) {
				if (label.module == a_name) return &label;
			}
			return nullptr;
		}

		constexpr ImVec4 kDim{ 0.70f, 0.70f, 0.70f, 1.0f };
		constexpr ImVec4 kAlert{ 0.95f, 0.55f, 0.20f, 1.0f };

		void RenderModule(const Module* a_module)
		{
			const auto name = a_module->Name();
			const auto* label = FindLabel(name);

			bool on = Settings::Enabled(name);
			const char* title = label && label->title ? label->title : name.data();
			if (ImGui::Checkbox(title, &on)) {
				Settings::SetEnabled(name, on);
			}

			// Authors read the internal name anyway; in release it is duplicate chrome.
			if constexpr (!kRelease) {
				ImGui::SameLine();
				ImGui::TextColored(kDim, "(%s)", name.data());
			}

			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);

			if (label && label->what && *label->what) {
				ImGui::TextColored(kDim, "%s", label->what);
			}

			if (label && label->needs && *label->needs) {
				ImGui::TextColored(kDim, "依赖模组: %s", label->needs);
			}

			if (!on) {
				ImGui::TextColored(kDim, "已关闭。不显示提示");
			} else if constexpr (!kRelease) {
				// Author-side: the live gate inputs and the last log line, so a missing prompt is
				// explained without opening the log.
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

		// The framework lists a section's items by their names, so the numbers fix the order.
		constexpr auto kPageModules = "1. 模块";
		constexpr auto kPageKeys = "2. 快捷键";
		constexpr auto kPageOptions = "3. 详细设置";

		void RenderKeys()
		{
			const auto keys = Settings::PromptKeys();
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"设置用于触发屏幕提示的键盘按键（DirectInput 扫描码）。手柄使用默认按键。");
			ImGui::PopTextWrapPos();
			ImGui::Spacing();

			for (std::size_t i = 0; i < keys.size(); ++i) {
				const auto label = std::format("按键插槽 {}", i + 1);
				int key = static_cast<int>(keys[i]);
				if (ImGui::InputInt(label.c_str(), &key)) {
					Settings::SetPromptKey(i, static_cast<std::uint32_t>(key));
				}
				ImGui::SameLine();
				ImGui::TextColored(kDim, "(DX 扫描码: %u)", keys[i]);
			}
		}

		void RenderPromptOnlyTarget(const char* a_label, std::string_view a_target)
		{
			bool on = Settings::PromptOnly(a_target);
			if (ImGui::Checkbox(a_label, &on)) {
				Settings::SetPromptOnly(a_target, on);
			}
			const auto key = Settings::ManualKey(a_target);
			ImGui::SameLine();
			ImGui::TextColored(kDim, "(原按键: %d)", key);
		}

		void RenderPromptOnly()
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"开启后，将拦截这些模组的原快捷键，使其仅通过 CIGAR 屏幕提示触发，防止按键冲突。");
			ImGui::PopTextWrapPos();
			ImGui::Spacing();

			RenderPromptOnlyTarget("Valhalla Combat 处决", "valhalla");
			RenderPromptOnlyTarget("Acheron 投降", "surrender");
			RenderPromptOnlyTarget("Grapple 擒拿", "grapple");
			RenderPromptOnlyTarget("Fill Her Up 排出", "fillherup");
		}

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
			ImGui::SeparatorText("模组快捷键重定向");
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
			ImGui::TextColored(kDim, "默认 3 阶段。非战斗中达到此饥饿阶段时弹出最便宜食物的进食提示。");

			ImGui::SeparatorText("排泄");
			int needs = Settings::NeedsMinPercent();
			if (ImGui::SliderInt("弹出排泄提示的蓄积百分比", &needs, Needs::kMinPercentLow, Needs::kMinPercentHigh, "%d%%")) {
				Settings::SetNeedsMinPercent(needs);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 50%%。Private Needs 膀胱/肠道蓄积度达到该数值时弹出排泄提示。");

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

				bar("生命提示阈值##pot-hp-th", tune.healthThreshold, "默认 50%。生命值低于此比例时提示喝生命药水。");
				bar("生命危机阈值##pot-hp-urgent", tune.urgentHealthThreshold, "默认 20%。低于此比例时优先选择强效生命药水。");
				bar("耐力提示阈值##pot-sp-th", tune.staminaThreshold, "默认 50%。");
				bar("法力提示阈值##pot-mp-th", tune.magickaThreshold, "默认 50%。");
			}

			ImGui::SeparatorText("武器切换");
			float swap = Settings::WeaponSwapRange();
			if (ImGui::SliderFloat("切换判定距离", &swap, WeaponSwap::kRangeLow, WeaponSwap::kRangeHigh, "%.0f")) {
				Settings::SetWeaponSwapRange(swap);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 800。敌人在此距离外或逃跑时提示远程武器，在此距离内提示近战武器。");

			ImGui::SeparatorText("柔术");
			float reach = Settings::JujutsuReach();
			if (ImGui::SliderFloat("柔术有效距离", &reach, Jujutsu::kReachLow, Jujutsu::kReachHigh, "%.0f")) {
				Settings::SetJujutsuReach(reach);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 250。格挡中的人形敌人在该距离内时弹出柔术提示。");

			auto tune = Settings::JujutsuTune();
			float guardPct = tune.guardStun * 100.0f;
			if (ImGui::SliderFloat("失衡条伤害", &guardPct, 0.0f, 100.0f, "%.0f%%")) {
				tune.guardStun = guardPct / 100.0f;
				Settings::SetJujutsuTune(tune);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 15%%。对格挡目标的失衡槽造成的伤害百分比。");

			ImGui::SeparatorText("脱衣·穿衣");
			float range = Settings::PlaceRange();
			if (ImGui::SliderFloat("家具交互距离", &range, Settings::kPlaceRangeMin, Settings::kPlaceRangeMax, "%.0f")) {
				Settings::SetPlaceRange(range);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "默认 250。离开瞄准的床或衣柜超过此距离时取消提示。");
		}
	}

	void Register()
	{
		const auto framework = SKSEMenuFramework_Module();
		if (!framework) {
			logs::info("control panel: SKSE Menu Framework is not loaded; no panel");
			return;
		}

		// The header ignores a missing export silently, so check the one that matters here.
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
