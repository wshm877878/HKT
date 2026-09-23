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
			Label{ "Bathe", "목욕", "Bathing in Skyrim - Renewed",
				"물에 들어가면 목욕, 폭포 아래에서는 샤워 프롬프트가 뜹니다. 씻으면 때가 사라집니다." },
			Label{ "Dress", "탈의·착용", "",
				"침대나 옷장 앞, 그리고 물속에서 탈의 프롬프트가 뜹니다. 벗은 옷은 기억해 두었다가 착용 프롬프트로 그대로 입습니다." },
			Label{ "BaboKey", "납치 행동 선택", "BaboDialogue",
				"납치당한 방에서 행동 선택 프롬프트가 뜹니다. 단축키 대신 프롬프트로 고릅니다." },
			Label{ "LockOn", "록온", "True Directional Movement",
				"전투 중 적을 록온하는 프롬프트가 뜹니다. 이미 록온 중이면 뜨지 않습니다." },
			Label{ "Grapple", "그래플", "Grapple (Patreon)",
				"전투 중 가까운 적에게 그래플 프롬프트가 뜹니다. 록온 상태에서 쓰면 그래플이 끝난 뒤 다시 록온합니다." },
			Label{ "Deflate", "배출", "Fill Her Up",
				"몸에 찬 것을 배출하는 프롬프트가 뜹니다. 키를 길게 누릅니다." },
			Label{ "Surrender", "항복", "Acheron (Yamete Kudasai)",
				"전투 중 체력이 40% 아래로 떨어지면 항복 프롬프트가 뜹니다. 키를 길게 누르며, 누르는 동안 화면이 느려집니다." },
			Label{ "Eat", "먹기", "Survival Mode (SMI, Gourmet)",
				"배가 고프면 가진 음식 중 가장 싼 것을 먹는 프롬프트가 뜹니다. 날고기, 술, 상한 음식은 고르지 않습니다." },
			Label{ "WeaponSwap", "무기 전환", "",
				"적이 멀거나 도망치면 원거리 무기로, 가까우면 근접 무기로 바꾸는 프롬프트가 뜹니다. 쓰기 전 무기로 되돌아옵니다." },
			Label{ "Execute", "처형", "Valhalla Combat",
				"스태거가 깨진 적에게 처형 프롬프트가 뜹니다. 프롬프트는 실제로 처형이 나갈 때만 보입니다." },
			Label{ "Jujutsu", "유술", "",
				"가드 중인 인간형 적에게 유술 프롬프트가 뜹니다. 적을 죽이지 않고 넘어뜨리며, 가드를 무너뜨립니다." },
			Label{ "Needs", "용변", "Private Needs - Orgasm",
				"방광이나 장이 차면 용변 프롬프트가 뜹니다. 전투 중, 물속, 앉은 상태에서는 뜨지 않습니다." },
			Label{ "Potion", "물약", "",
				"체력·기력·마나가 부족하거나 중독·질병 상태이거나 물속에 잠겼을 때, 알맞은 물약을 마시는 프롬프트가 뜹니다. 아까운 물약을 먼저 쓰지 않습니다." },
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
			std::uint32_t code;  // DirectInput scan code
			const char* name;
		};

		// The keys offered for prompt slots: the ones SkyPrompt has icons for and a player can reach
		// without leaving the movement keys for long.
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
				return std::format("마우스 {}", a_code - 255);
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
			const auto manualName = manual >= 0 ? NameOf(manual) : std::string("기록 없음");
			if (on) {
				ImGui::TextColored(kDim, "모드 키를 %s(숨김 키)로 옮김. 원래 키 %s는 비어 있음", a_hidden, manualName.c_str());
			} else {
				ImGui::TextColored(kDim, "모드 자체 키 사용. 끌 때 복원한 키: %s", manualName.c_str());
			}
			ImGui::Unindent();
		}

		void RenderPromptOnly()
		{
			RenderPromptOnlyItem("grapple", "그래플: 프롬프트 전용##po-grapple", "F13",
				[] { Grapple::GetSingleton()->CheckKeys(); });
			RenderPromptOnlyItem("surrender", "Acheron 항복: 프롬프트 전용##po-surrender", "F14",
				[] { Surrender::GetSingleton()->ApplyKeyMode(); });
			RenderPromptOnlyItem("valhalla", "Valhalla 처형: 프롬프트 전용##po-valhalla", "F15",
				[] { Execute::GetSingleton()->CheckKey(); });
			{
				bool on = Settings::PromptOnly("privateneeds");
				if (ImGui::Checkbox("Private Needs: 프롬프트 전용##po-privateneeds", &on)) {
					Settings::SetPromptOnly("privateneeds", on);
					SKSE::GetTaskInterface()->AddTask([] { Needs::GetSingleton()->ApplyKeyMode(); });
				}
				ImGui::Indent();
				const auto keys = Needs::GetSingleton()->KeySummary();
				if (on) {
					ImGui::TextColored(kDim, "PNO 단축키 6개 해제(메뉴 Y, 수치 확인 U 포함). MCM을 닫을 때마다 다시 확인");
				} else {
					ImGui::TextColored(kDim, "PNO 자체 키 사용. 현재 키 코드: %s", keys.empty() ? "없음" : keys.c_str());
				}
				ImGui::Unindent();
			}
			if (ImGui::Button("모드 키 다시 확인")) {
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
			ImGui::TextColored(kDim, "현재: 그래플 %s, Acheron 항복 %s, Valhalla 처형 %s",
				grapple >= 0 ? NameOf(grapple).c_str() : "없음", surrender >= 0 ? NameOf(surrender).c_str() : "없음",
				execution >= 0 ? NameOf(execution).c_str() : "없음");
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kDim, "키는 불러오기 때와 이 버튼을 누를 때만 확인. MCM에서 키를 바꾼 뒤 누를 것. 프롬프트 전용이 켜져 있으면 바꾼 키를 기억하고 숨김 키로 되돌림");
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
				const auto label = std::format("{}번째 프롬프트 키##key{}", slot + 1, slot);
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::Combo(label.c_str(), &current, names.data(), static_cast<int>(names.size()), 12) && current >= 0) {
					Settings::SetPromptKey(slot, kKeys[current].code);
				}
				if (current < 0) {
					ImGui::SameLine();
					ImGui::TextColored(kDim, "(설정 파일 값 %s)", NameOf(keys[slot]).c_str());
				}
			}
			if (ImGui::Button("기본값 (1, 2, 3, 4)")) {
				for (std::size_t slot = 0; slot < keys.size(); ++slot) {
					if (keys[slot] != Settings::kDefaultPromptKeys[slot]) {
						Settings::SetPromptKey(slot, Settings::kDefaultPromptKeys[slot]);
					}
				}
			}
			ImGui::TextColored(kDim, "화면에 뜬 순서대로 1번째부터 배정. 게임패드는 SkyPrompt 기본값");

			// A key shared by two slots fires both prompts; a key another mod listens to fires that mod too.
			for (std::size_t a = 0; a < keys.size(); ++a) {
				for (std::size_t b = a + 1; b < keys.size(); ++b) {
					if (keys[a] == keys[b]) {
						ImGui::TextColored(kWarn, "경고: %zu번째와 %zu번째 키가 같음 (%s)", a + 1, b + 1, NameOf(keys[a]).c_str());
					}
				}
			}
			const std::array<std::pair<const char*, std::int64_t>, 3> others{ {
				{ "그래플", Grapple::GetSingleton()->Key() },
				{ "Acheron 항복", Surrender::GetSingleton()->SurrenderKey() },
				{ "Valhalla 처형", Execute::GetSingleton()->ExecutionKey() },
			} };
			for (std::size_t slot = 0; slot < keys.size(); ++slot) {
				for (const auto& [who, code] : others) {
					if (code == keys[slot]) {
						ImGui::TextColored(kWarn, "경고: %zu번째 키(%s)가 %s 키와 같음", slot + 1, NameOf(code).c_str(), who);
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
				ImGui::TextColored(kDim, "연동: %s (없으면 대기)", label->needs);
			}
			if (!on) {
				ImGui::TextColored(kDim, "꺼짐. 프롬프트 표시 안 함");
			} else if constexpr (!kRelease) {
				// Author-side: the live gate inputs and the last log line, so a missing prompt is
				// explained without opening the log.
				const auto gate = a_module->ShownGate();
				const auto line = a_module->ShownLine();
				ImGui::TextColored(kDim, "조건: %s", gate.empty() ? "기록 없음" : gate.c_str());
				ImGui::TextColored(kDim, "최근: %s", line.empty() ? "기록 없음" : line.c_str());
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
		constexpr auto kPageModules = "1. 모듈";
		constexpr auto kPageKeys = "2. 단축키";
		constexpr auto kPageOptions = "3. 세부 설정";

		void __stdcall RenderModules()
		{
			LogFirstDraw(kPageModules);
			ImGui::SeparatorText("모듈");
			for (const auto* module : Modules()) {
				RenderModule(module);
			}

			ImGui::SeparatorText("상태");
			ImGui::Text("SkyPrompt: %s", Prompts::Available() ? "연결됨" : "없음 (프롬프트 비활성)");
			if constexpr (!kRelease) {
				const auto source = Settings::SourceDescription();
				ImGui::TextColored(kDim, "설정 파일: %s", source.c_str());
			}
			ImGui::TextColored(kDim, "문제가 생기면 SKSE\\CIGAR.log를 첨부해 주세요");
		}

		void __stdcall RenderKeyPage()
		{
			LogFirstDraw(kPageKeys);
			ImGui::SeparatorText("프롬프트 키");
			RenderKeys();

			ImGui::SeparatorText("모드 단축키");
			RenderPromptOnly();
		}

		void __stdcall RenderOptions()
		{
			LogFirstDraw(kPageOptions);
			ImGui::SeparatorText("먹기");
			int stage = Settings::EatMinStage();
			if (ImGui::SliderInt("표시 시작 허기 단계", &stage, Eat::kMinStageLow, Eat::kMinStageHigh)) {
				Settings::SetEatMinStage(stage);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 3. 비전투 중 이 단계 이상이면 가장 싼 음식으로 프롬프트 표시");

			ImGui::SeparatorText("용변");
			int needs = Settings::NeedsMinPercent();
			if (ImGui::SliderInt("표시 시작 수치", &needs, Needs::kMinPercentLow, Needs::kMinPercentHigh, "%d%%")) {
				Settings::SetNeedsMinPercent(needs);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 50%%. Private Needs의 방광·장 수치가 이 이상이면 프롬프트 표시");

			ImGui::SeparatorText("물약");
			{
				auto tune = Settings::PotionTune();
				bool changed = false;
				changed |= ImGui::Checkbox("체력##pot-hp", &tune.health);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("기력##pot-sp", &tune.stamina);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("마나##pot-mp", &tune.magicka);
				changed |= ImGui::Checkbox("해독##pot-poison", &tune.curePoison);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("질병 치료##pot-disease", &tune.cureDisease);
				ImGui::SameLine();
				changed |= ImGui::Checkbox("수중 호흡##pot-water", &tune.waterBreathing);
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
				bar("체력 표시 시작##pot-hp-th", tune.healthThreshold, "기본 50%. 체력이 이 비율 이하면 프롬프트 표시");
				bar("체력 위급##pot-hp-urgent", tune.urgentHealthThreshold, "기본 20%. 이 이하면 가장 약한 물약 대신 가장 강한 물약을 선택");
				bar("기력 표시 시작##pot-sp-th", tune.staminaThreshold, "기본 50%");
				bar("마나 표시 시작##pot-mp-th", tune.magickaThreshold, "기본 50%");
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextColored(kDim, "물약은 효과(회복하는 수치, 해독·질병 치료 원형)로 판별. 해로운 효과가 하나라도 있으면 제외. 한 번에 한 개만 표시하며 순서는 체력, 수중 호흡, 기력, 마나, 해독, 질병 치료");
				ImGui::PopTextWrapPos();
			}

			ImGui::SeparatorText("무기 전환");
			float swap = Settings::WeaponSwapRange();
			if (ImGui::SliderFloat("전환 거리", &swap, WeaponSwap::kRangeLow, WeaponSwap::kRangeHigh, "%.0f")) {
				Settings::SetWeaponSwapRange(swap);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 800. 적이 이 거리 밖이거나 도주 중이면 원거리, 안이면 근접 무기 프롬프트");

			ImGui::SeparatorText("유술");
			float reach = Settings::JujutsuReach();
			if (ImGui::SliderFloat("유술 거리", &reach, Jujutsu::kReachLow, Jujutsu::kReachHigh, "%.0f")) {
				Settings::SetJujutsuReach(reach);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 250. 가드 중인 인간형 적이 이 거리 안이면 프롬프트 표시");

			auto tune = Settings::JujutsuTune();
			float guardPct = tune.guardStun * 100.0f;
			if (ImGui::SliderFloat("게이지 피해", &guardPct, 0.0f, 100.0f, "%.0f%%")) {
				tune.guardStun = guardPct / 100.0f;
				Settings::SetJujutsuTune(tune);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 15%%. 발할라 최대 스태거 게이지 대비. 래그돌은 별도");

			ImGui::SeparatorText("탈의·착용");
			float range = Settings::PlaceRange();
			if (ImGui::SliderFloat("침대·옷장 유효 거리", &range, Settings::kPlaceRangeMin, Settings::kPlaceRangeMax, "%.0f")) {
				Settings::SetPlaceRange(range);
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				Settings::Save();
			}
			ImGui::TextColored(kDim, "기본 250. 조준한 가구에서 이 거리를 벗어나면 프롬프트 해제");
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
