#include "Hooks/hooks.h"

#include "RE/Misc.h"

namespace Hooks {
	void DialogueItemConstructorCall::Install()
	{
		auto& trampoline = SKSE::GetTrampoline();

		REL::Relocation<std::uintptr_t> playerGoldTarget{ REL::ID(25541), 0xE2 };
		_createDialogueItem = trampoline.write_call<5>(playerGoldTarget.address(), &CreateDialogueItem);
	}

	void DialogueItemConstructorCall::SetMaxDistance(double a_newDistance)
	{
		if (a_newDistance < 0.0) {
			a_newDistance = 0.0;
		}
		else if (a_newDistance > 2000.0f) {
			a_newDistance = 2000.0f;
		}

		maximumDistance = static_cast<float>(a_newDistance);
	}

	void DialogueItemConstructorCall::RegisterWhitelistedActor(const RE::TESNPC* a_actor)
	{
		assert(a_actor);
		if (std::find(whitelistedActors.begin(), whitelistedActors.end(), a_actor) != whitelistedActors.end()) {
			return;
		}

		whitelistedActors.push_back(a_actor);
	}

	void DialogueItemConstructorCall::RegisterWhitelistedQuest(const RE::TESQuest* a_quest)
	{
		assert(a_quest);
		if (std::find(whitelistedQuests.begin(), whitelistedQuests.end(), a_quest) != whitelistedQuests.end()) {
			return;
		}

		whitelistedQuests.push_back(a_quest);
	}

	RE::DialogueItem* DialogueItemConstructorCall::CreateDialogueItem(
		RE::DialogueItem* a_this, 
		RE::TESQuest* a_quest,
		RE::TESTopic* a_topic,
		RE::TESTopicInfo* a_topicInfo,
		RE::TESObjectREFR* a_speaker)
	{
		const auto response = _createDialogueItem(a_this, a_quest, a_topic, a_topicInfo, a_speaker);
		const auto singleton = DialogueItemConstructorCall::GetSingleton();

		if (a_speaker && a_speaker->As<RE::Actor>()) {
			const auto speakerActor = a_speaker->As<RE::Actor>();
			const auto speakerBase = speakerActor->GetActorBase();

			singleton->UpdateInternalClosestConversation(speakerActor);

			if (speakerActor->IsPlayerTeammate()) {
				if (std::find(singleton->whitelistedQuests.begin(), singleton->whitelistedQuests.end(), a_quest) != singleton->whitelistedQuests.end()) {
					return response;
				}
				if (std::find(singleton->whitelistedActors.begin(), singleton->whitelistedActors.end(), speakerBase) != singleton->whitelistedActors.end()) {
					return response;
				}
				if (singleton->ShouldFollowerRespond(speakerActor, a_topic)) {
					return response;
				}
				else {
					delete a_this;
					return nullptr;
				}
			}
		}
		return response;
	}

	bool DialogueItemConstructorCall::ShouldFollowerRespond(RE::Actor* a_speaker, RE::TESTopic* a_topic)
	{
		using DialogueData = RE::DIALOGUE_DATA::Subtype;
		using DialogueType = RE::DIALOGUE_TYPE;
		bool talkingToPlayer = a_speaker->GetHighProcess() ? a_speaker->GetHighProcess()->talkingToPC : false;
		if (talkingToPlayer) {
			return true;
		}

		switch (a_topic->data.subtype.get()) {
		case DialogueData::kAlertIdle:
		case DialogueData::kAlertToCombat:
		case DialogueData::kBash:
		case DialogueData::kCombatGrunt:
		case DialogueData::kDeath:
		case DialogueData::kForceGreet:
		case DialogueData::kHit:
		case DialogueData::kLostToCombat:
		case DialogueData::kNormalToCombat:
		case DialogueData::kVoicePowerEndLong:
		case DialogueData::kVoicePowerStartLong:
		case DialogueData::kVoicePowerEndShort:
		case DialogueData::kVoicePowerStartShort:
			return true;
		default:
			break;
		}

		switch (a_topic->data.type.get()) {
		case DialogueType::kSceneDialogue:
			return true;
		default:
			break;
		}

		const auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
		assert(menuTopicManager);
		const auto currentPlayerDialogueTarget = menuTopicManager->speaker.get().get();
		if (currentPlayerDialogueTarget && currentPlayerDialogueTarget != a_speaker) {
			return false;
		}

		const auto closestSpeakingCharacter = closestSpeaker->As<RE::Character>();
		bool isClosestActorSpeaking = closestSpeakingCharacter ? RE::IsTalking(closestSpeakingCharacter) : false;
		if (isClosestActorSpeaking && closestSpeaker && closestSpeaker->Is3DLoaded()) {
			const auto player = RE::PlayerCharacter::GetSingleton();
			assert(player);
			const auto distance = player->GetDistance(closestSpeaker);
			if (distance < maximumDistance && !closestSpeaker->QSpeakingDone()) {
				return false;
			}
		}
		return true;
	}

	void DialogueItemConstructorCall::UpdateInternalClosestConversation(RE::Actor* a_speaker)
	{
		if (a_speaker == RE::PlayerCharacter::GetSingleton()) {
			return;
		}

		if (!a_speaker->IsPlayerTeammate()) {
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto speakerCharacter = a_speaker->As<RE::Character>();
			if (!speakerCharacter) {
				return;
			}

			assert(player);
			bool shouldReplace = false;
			const auto oldSpeakerDone = !RE::IsTalking(speakerCharacter);
			if (oldSpeakerDone) {
				shouldReplace = true;
			}
			if (!shouldReplace) {
				if (!closestSpeaker || !closestSpeaker->Is3DLoaded()) {
					shouldReplace = true;
				}
			}
			if (!shouldReplace) {
				const auto newDistance = player->GetDistance(a_speaker);
				const auto oldDistance = player->GetDistance(closestSpeaker);
				if (newDistance < oldDistance) {
					shouldReplace = true;
				}
			}

			if (shouldReplace) {
				closestSpeaker = a_speaker;
			}
		}
	}

	void Install()
	{
		DialogueItemConstructorCall::GetSingleton()->Install();
	}
}