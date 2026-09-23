#include "Prompt.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <Windows.h>

#include "Module.h"
#include "Settings.h"

namespace CIGAR
{
	namespace
	{
		SkyPromptAPI::ClientID clientID = 0;

		constexpr RE::FormID kPlayerRef = 0x14;
		// SkyPrompt fades a prompt out after its lifetime setting; re-sending well within it keeps
		// the prompt up.
		constexpr auto kKeepAliveInterval = 2s;

		struct Translation
		{
			std::string from;
			std::string to;
		};

		std::vector<Translation> translations;

		std::filesystem::path TranslationFilePath()
		{
			wchar_t buffer[32768]{};
			const auto length = ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
			if (length > 0 && length < std::size(buffer)) {
				return std::filesystem::path(buffer, buffer + length).parent_path() /
					L"Data" / L"Interface" / L"Translations" / L"CIGAR_CHINESE.txt";
			}
			return std::filesystem::path("Data") / "Interface" / "Translations" / "CIGAR_CHINESE.txt";
		}

		std::string Trim(std::string a_text)
		{
			const auto isSpace = [](unsigned char a_c) {
				return a_c == ' ' || a_c == '\t' || a_c == '\r' || a_c == '\n';
			};
			auto first = a_text.begin();
			while (first != a_text.end() && isSpace(static_cast<unsigned char>(*first))) {
				++first;
			}
			auto last = a_text.end();
			while (last != first && isSpace(static_cast<unsigned char>(*(last - 1)))) {
				--last;
			}
			return std::string(first, last);
		}

		void LoadTranslations()
		{
			translations.clear();

			const auto path = TranslationFilePath();
			std::ifstream file(path, std::ios::binary);
			if (!file) {
				logs::info("CIGAR Chinese translation file not found: {}", path.string());
				return;
			}

			std::string line;
			std::size_t loaded = 0;
			while (std::getline(file, line)) {
				if (loaded == 0 && line.size() >= 3 &&
					static_cast<unsigned char>(line[0]) == 0xEF &&
					static_cast<unsigned char>(line[1]) == 0xBB &&
					static_cast<unsigned char>(line[2]) == 0xBF) {
					line.erase(0, 3);
				}
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				if (line.empty() || line.front() == '#') {
					continue;
				}

				auto separator = line.find('\t');
				if (separator == std::string::npos) {
					separator = line.find('=');
				}
				if (separator == std::string::npos) {
					logs::warn("Ignoring malformed CIGAR translation line");
					continue;
				}

				auto from = Trim(line.substr(0, separator));
				auto to = Trim(line.substr(separator + 1));
				if (from.empty() || to.empty()) {
					continue;
				}

				translations.push_back({ std::move(from), std::move(to) });
				++loaded;
			}

			std::sort(translations.begin(), translations.end(), [](const Translation& a_lhs, const Translation& a_rhs) {
				return a_lhs.from.size() > a_rhs.from.size();
			});
			logs::info("Loaded {} CIGAR Chinese translation entries from {}", translations.size(), path.string());
		}

		std::string TranslateText(std::string a_text)
		{
			for (const auto& translation : translations) {
				std::size_t pos = 0;
				while ((pos = a_text.find(translation.from, pos)) != std::string::npos) {
					a_text.replace(pos, translation.from.size(), translation.to);
					pos += translation.to.size();
				}
			}
			return a_text;
		}

		const char* EventName(SkyPromptAPI::PromptEventType a_type)
		{
			switch (a_type) {
			case SkyPromptAPI::kAccepted:
				return "accepted";
			case SkyPromptAPI::kDeclined:
				return "declined";
			case SkyPromptAPI::kRemovedByMod:
				return "removed";
			case SkyPromptAPI::kTimingOut:
				return "timing-out";
			case SkyPromptAPI::kTimeout:
				return "timeout";
			case SkyPromptAPI::kDown:
				return "down";
			case SkyPromptAPI::kUp:
				return "up";
			case SkyPromptAPI::kMove:
				return "move";
			default:
				return "unknown";
			}
		}
	}

	namespace
	{
		// Every slot, so a module switched off in the control panel can be cleared from outside.
		std::vector<std::pair<const Module*, PromptSlot*>>& Slots()
		{
			static std::vector<std::pair<const Module*, PromptSlot*>> slots;
			return slots;
		}

		bool duplicateIDs = false;

		// Which event ID holds each key slot (0 = free; event IDs start at 1). Game thread only.
		std::array<SkyPromptAPI::EventID, Settings::kPromptKeyCount> keySlots{};

		// The slot this event already holds, or the lowest free one; -1 when all are taken.
		int AcquireKeySlot(SkyPromptAPI::EventID a_id)
		{
			for (std::size_t i = 0; i < keySlots.size(); ++i) {
				if (keySlots[i] == a_id) {
					return static_cast<int>(i);
				}
			}
			for (std::size_t i = 0; i < keySlots.size(); ++i) {
				if (keySlots[i] == 0) {
					keySlots[i] = a_id;
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		void ReleaseKeySlot(SkyPromptAPI::EventID a_id)
		{
			for (auto& holder : keySlots) {
				if (holder == a_id) {
					holder = 0;
				}
			}
		}
	}

	bool Prompts::Init()
	{
		LoadTranslations();

		if (clientID == 0) {
			clientID = SkyPromptAPI::RequestClientID();
		}
		logs::info("SkyPrompt client id {} (API {}.{})", clientID, SkyPromptAPI::MAJOR, SkyPromptAPI::MINOR);
		// A shared event ID makes one key press fire every prompt that uses it.
		std::map<SkyPromptAPI::EventID, std::string> owners;
		for (const auto& [owner, slot] : Slots()) {
			const auto [it, fresh] = owners.emplace(slot->ID(), owner->Name());
			if (!fresh) {
				logs::error("prompt event id {} is used by both {} and {}", slot->ID(), it->second, owner->Name());
				duplicateIDs = true;
			}
		}
		return clientID != 0;
	}

	bool Prompts::Available() { return clientID != 0; }

	SkyPromptAPI::ClientID Prompts::Client() { return clientID; }

	bool Prompts::HasDuplicateIDs() { return duplicateIDs; }

	void Prompts::WithdrawAll(const Module* a_owner)
	{
		for (auto [owner, slot] : Slots()) {
			if (owner == a_owner) {
				slot->Reset();
				slot->Withdraw();
			}
		}
	}

	void Prompts::WithdrawEverything()
	{
		for (auto [owner, slot] : Slots()) {
			slot->Reset();
			slot->Withdraw();
		}
		keySlots.fill(0);
	}

	PromptSlot::PromptSlot(Module* a_owner, SkyPromptAPI::EventID a_id) :
		owner(a_owner),
		id(a_id)
	{
		Slots().emplace_back(a_owner, this);
	}

	void PromptSlot::Update(bool a_can, const std::function<std::string()>& a_text)
	{
		if (a_can && !offered) {
			offered = true;
			Offer(a_text());
		} else if (a_can) {
			KeepAlive();
		} else if (offered) {
			offered = false;
			Withdraw();
		}
	}

	void PromptSlot::Offer(std::string a_text)
	{
		if (!Prompts::Available()) {
			return;
		}

		// Translate once, immediately before the text reaches SkyPrompt.
		a_text = TranslateText(std::move(a_text));

		// SkyPrompt reads the prompt later through GetPrompts(), so the text must outlive this call.
		text = std::move(a_text);
		// The keyboard key comes from CIGAR's settings; a device without a listed key (the gamepad)
		// gets SkyPrompt's default for the slot SkyPrompt picks. SkyPrompt keeps a queued prompt's key,
		// so the slot is held until the prompt is withdrawn.
		const int slot = AcquireKeySlot(id);
		std::span<const std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> keys;
		std::uint32_t key = 0;
		if (slot >= 0) {
			key = Settings::PromptKeys()[slot];
			buttons[0] = { RE::INPUT_DEVICE::kKeyboard, key };
			keys = buttons;
		}
		prompts[0] = SkyPromptAPI::Prompt(text, id, 0, promptType, kPlayerRef, keys, color);
		const bool sent = SkyPromptAPI::SendPrompt(this, clientID);
		lastSent = std::chrono::steady_clock::now();
		owner->Log("offer event={} '{}' slot={} key={} sent={}", id, text, slot + 1, key, sent);
	}

	void PromptSlot::KeepAlive()
	{
		// Re-sending a queued prompt makes SkyPrompt reset its lifetime (IsInQueue -> WakeUpQueue).
		const auto now = std::chrono::steady_clock::now();
		if (!Prompts::Available() || now - lastSent < kKeepAliveInterval) {
			return;
		}
		lastSent = now;
		static_cast<void>(SkyPromptAPI::SendPrompt(this, clientID));
	}

	void PromptSlot::SetColor(std::uint32_t a_color)
	{
		if (color == a_color) {
			return;
		}
		color = a_color;
		if (offered && Prompts::Available()) {
			// SkyPrompt refreshes text, colour and progress of a prompt that is already queued.
			prompts[0].text_color = color;
			lastSent = std::chrono::steady_clock::now();
			static_cast<void>(SkyPromptAPI::SendPrompt(this, clientID));
		}
	}

	void PromptSlot::Withdraw()
	{
		if (Prompts::Available()) {
			SkyPromptAPI::RemovePrompt(this, clientID);
		}
		ReleaseKeySlot(id);
	}

	std::span<const SkyPromptAPI::Prompt> PromptSlot::GetPrompts() const
	{
		return prompts;
	}

	void PromptSlot::ProcessEvent(SkyPromptAPI::PromptEvent a_event) const
	{
		// Called from SkyPrompt's thread: log, then hand the action to the game thread.
		const auto type = a_event.type;
		const auto eventID = a_event.prompt.eventID;
		const auto module = owner;
		// Timing-out and move arrive once per frame; logging them buried everything else.
		if (type != SkyPromptAPI::kTimingOut && type != SkyPromptAPI::kMove) {
			logs::info("[{}] prompt event {} ({}) event={}", module->Name(), EventName(type), static_cast<int>(type), eventID);
		}
		auto* self = const_cast<PromptSlot*>(this);
		if (type == SkyPromptAPI::kTimeout) {
			// Faded out despite the keep-alive (e.g. while paused): offer again on the next tick.
			SKSE::GetTaskInterface()->AddTask([self]() {
				self->Reset();
				ReleaseKeySlot(self->ID());
			});
		}
		if (hold) {
			const bool down = type == SkyPromptAPI::kDown;
			const bool ends = type == SkyPromptAPI::kUp || type == SkyPromptAPI::kRemovedByMod ||
			                  type == SkyPromptAPI::kTimeout || type == SkyPromptAPI::kDeclined;
			if (down || ends) {
				SKSE::GetTaskInterface()->AddTask([module, eventID, down]() { module->OnHold(eventID, down); });
			}
			return;
		}
		if (type != SkyPromptAPI::kAccepted) {
			return;
		}
		SKSE::GetTaskInterface()->AddTask([self, module, eventID]() {
			self->Withdraw();
			if (self->repeat) {
				self->Reset();
			}
			module->OnAccepted(eventID);
		});
	}
}
