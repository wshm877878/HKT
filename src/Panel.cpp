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
			const auto& keys = Settings::Keys();
			if (keys.empty()) {
				ImGui::TextColored(kDim, "未设置按键");
				return;
			}

			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"提示分配的 DirectInput 扫描码。可以在 CIGAR.json 中进行配置。");
			ImGui::PopTextWrapPos();
			ImGui::Spacing();

			for (std::size_t i = 0; i < keys.size(); ++i) {
				ImGui::Text("按键插槽 %zu:", i + 1);
				ImGui::SameLine();
				ImGui::TextColored(kDim, "0x%02X", keys[i]);
			}
		}

		void RenderPromptOnly()
		{
			const auto& map = Settings::PromptOnly();
			if (map.empty()) {
				ImGui::TextColored(kDim, "无被重定向的模组按键");
				return;
			}

			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"被 CIGAR 拦截并设为仅提示触发的模组原按键。关闭对应模块后将自动恢复。");
			ImGui::PopTextWrapPos();
			ImGui::Spacing();

			for (const auto& [mod, key] : map) {
				ImGui::Text("%s:", mod.c_str());
				ImGui::SameLine();
				ImGui::TextColored(kDim, "0x%02X", key);
			}
		}

		void RenderEatOptions()
		{
			bool inCombat = Settings::CombatEat();
			if (ImGui::Checkbox("允许战斗中进食", &inCombat)) {
				Settings::SetCombatEat(inCombat);
			}

			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"关闭后，战斗中即使饥饿也不会弹出进食提示。");
			ImGui::PopTextWrapPos();
			ImGui::Unindent();
		}

		void RenderDrinkOptions()
		{
			bool inCombat = Settings::CombatPotion();
			if (ImGui::Checkbox("允许战斗中喝药", &inCombat)) {
				Settings::SetCombatPotion(inCombat);
			}

			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"关闭后，战斗中即使生命值或魔法值降低也不会弹出喝药提示。");
			ImGui::PopTextWrapPos();
			ImGui::Unindent();
			ImGui::Spacing();

			bool overrule = Settings::OverruleStreamlinedInteractions();
			if (ImGui::Checkbox("禁用 Streamlined Interactions 的自带喝药提示", &overrule)) {
				Settings::SetOverruleStreamlinedInteractions(overrule);
			}

			ImGui::Indent();
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim,
				"若开启 SI 自带的喝药提示，可能会与 CIGAR 冲突叠字。开启此项后 CIGAR 将自动接管并禁用 SI 喝药功能。");
			ImGui::PopTextWrapPos();
			ImGui::Unindent();
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
			ImGui::SeparatorText("模组快捷键");
			RenderPromptOnly();
		}

		void __stdcall RenderOptions()
		{
			LogFirstDraw(kPageOptions);
			ImGui::SeparatorText("进食设置");
			RenderEatOptions();
			ImGui::SeparatorText("药水设置");
			RenderDrinkOptions();
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logs::warn("SKSE Menu Framework is not installed; control panel unavailable");
			return;
		}

		SKSEMenuFramework::SetSection(kSection);
		SKSEMenuFramework::AddSectionItem(kPageModules, RenderModules);
		SKSEMenuFramework::AddSectionItem(kPageKeys, RenderKeyPage);
		SKSEMenuFramework::AddSectionItem(kPageOptions, RenderOptions);
		logs::info("已将 CIGAR 成功注册到 SKSE Menu Framework");
	}
}
