// vip_smoke — Plugify port of the Pisex VIP module "VIP_Smoke" / "[VIP] Colored Smoke"
// (the original's vip_smoke.cpp, 207 lines).
//
// Behaviour kept identical: when a smokegrenade_projectile spawns and its thrower is a VIP with a
// non-empty "smoke_color" feature value, the projectile's m_vSmokeColor is set one frame later to
// the value of the feature: "R G B" (three space separated numbers, the original's splitter
// strips quotes and keeps quoted parts together) or the literal "random" for a random colour.
// A value that does not parse logs "[VIP-SMOKE] Invalid smoke color format: <value>".
//
// Port notes:
//   * The original implemented IEntityListener::OnEntitySpawned and added itself to the entity
//     system; s2sdk exposes the same engine hook as OnEntitySpawned_Register (a post hook on
//     IEntityListener::OnEntitySpawned) and passes the entity as the raw handle value, so the
//     original's dynamic cast to the smoke grenade projectile becomes a classname comparison
//     against "smokegrenade_projectile" and the captured handle is that value.
//   * The thrower (CBaseGrenade::m_hThrower, reachable from CSmokeGrenadeProjectile through the
//     base classes) is resolved by name through pisex/schema.hpp; IsAlive() is
//     m_lifeState == LIFE_ALIVE, like hl2sdk; and the controller's player slot is
//     s2sdk's EntPointerToPlayerSlot (utils::GetEntityPlayerSlot, which resolves the pawn's
//     m_hController and falls back to a controller scan).
//   * The registry lookup happens in the next frame (pisex::NextFrame -> pisex::Poll), like the
//     original; the entity is re-validated (index, classname) because a queued handle may have
//     been deleted or recycled in between.
//   * The write goes through pisex::SetEntVector with changeState = true (the pisexcompat
//     equivalent of writing the field and calling SetStateChanged): the original relied on the
//     freshly created entity's state still being sent, this makes the colour reach the clients
//     regardless of when the snapshot of the projectile went out.
//   * std::stoi of the original threw std::invalid_argument for a malformed number, which would
//     have terminated the server from this engine callback; the port parses non-throwing and logs
//     the same "Invalid smoke color format" line.
//   * The listener is registered in VIP_OnVIPLoaded (once the core's database is up) and removed
//     in OnPluginEnd, before this library is unmapped.

#include <pisex/pisex.hpp>
#include <pisex/schema.hpp>

#include "smoke_color.hpp"

#include <plg/numerics.hpp>
#include <plg/plugin.hpp>

#include <s2sdk/s2sdk.hpp>

#include <vip_api.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>

namespace {

// The GetLogTag() of the original ("[VIP-SMOKE]").
constexpr char kLogTag[] = "VIP-SMOKE";

// The entity classname of CSmokeGrenadeProjectile (what the original's dynamic cast matched).
constexpr char kSmokeGrenadeClass[] = "smokegrenade_projectile";

// The SCHEMA class of the projectile. pisex/schema.hpp lookups resolve class + field through
// s2sdk's per-class schema tables, which are keyed by the schema class name - an entity
// classname ("smokegrenade_projectile") is never found there ("Cannot find offset for ...", one
// console line per call). m_vSmokeColor is declared on this class and m_hThrower is inherited
// from CBaseGrenade; s2sdk collects base-class fields into the derived class, so both resolve
// here.
constexpr char kSmokeGrenadeSchemaClass[] = "CSmokeGrenadeProjectile";

// hl2sdk LIFE_ALIVE (LifeState_t): IsAlive() of the original.
constexpr int kLifeAlive = 0;

// CS2 has 64 player slots.
constexpr int kMaxSlots = 64;

// The object the ported module uses (the Pisex g_pVIPCore).
vip::IVIPApi* g_pVIPCore = &vip::IVIPApi::Get();

// --- the original's string helpers -------------------------------------------------------------
// SplitStringBySpace / the three-number parse and their tests live in smoke_color.hpp (the same
// file the unit test includes).

class VipSmokePlugin final : public plg::Plugin {
public:
	plg::PluginResult OnPluginStart() override;
	plg::PluginResult OnPluginUpdate(std::chrono::milliseconds dt) override;
	plg::PluginResult OnPluginEnd() override;

	// VIP_OnVIPLoaded of the original: the core's database is up, listen for smoke projectiles.
	void OnCoreReady();

	// The body of the original's OnEntitySpawned, one frame later. Public: the s2sdk listener is a
	// free function (a plain function pointer cannot be a member closure).
	void ApplySmokeColor(int iEntityHandle);

private:
	bool m_coreReady = false;
	bool m_listening = false;
};

VipSmokePlugin g_plugin;

// The IEntityListener::OnEntitySpawned of the original; s2sdk hands it the entity handle.
void OnEntitySpawnedListener(int32_t entityHandle) {
	try {
		const int iEntity = pisex::EntHandleToEntIndex(entityHandle);
		if (iEntity <= 0) {
			return;
		}
		// The original's cast to the smoke projectile type and its null check.
		if (pisex::GetEntityClassname(iEntity) != kSmokeGrenadeClass) {
			return;
		}
		// The original deferred the rest to the next frame, with the projectile's handle captured.
		pisex::NextFrame([entityHandle] { g_plugin.ApplySmokeColor(entityHandle); });
	} catch (const std::exception& ex) {
		pisex::ErrorLog("[%s] OnEntitySpawned failed: %s", kLogTag, ex.what());
	} catch (...) {
		pisex::ErrorLog("[%s] OnEntitySpawned failed with an unknown exception", kLogTag);
	}
}

// --- lifecycle ---------------------------------------------------------------------------------

plg::PluginResult VipSmokePlugin::OnPluginStart() {
	try {
		pisex::Init("vip");

		g_pVIPCore->VIP_OnVIPLoaded([] { g_plugin.OnCoreReady(); });
		if (g_pVIPCore->VIP_IsVIPLoaded()) {
			OnCoreReady();
		}

		// The original registered "smoke_color" (bool, togglable) in AllPluginsLoaded. The feature
		// is a bool in the menu but its value is the "R G B" string read below - exactly the
		// original's (mis)use, which the core reproduces by returning the raw group value.
		g_pVIPCore->VIP_RegisterFeature("smoke_color", vip::VIP_BOOL, vip::TOGGLABLE);

		return {};
	} catch (const std::exception& ex) {
		pisex::ErrorLog("[%s] OnPluginStart failed: %s", kLogTag, ex.what());
	} catch (...) {
		pisex::ErrorLog("[%s] OnPluginStart failed with an unknown exception", kLogTag);
	}
	return {}; // never Failed: that stops every plugify plugin
}

plg::PluginResult VipSmokePlugin::OnPluginUpdate(std::chrono::milliseconds dt) {
	(void)dt;
	try {
		pisex::Poll();
	} catch (const std::exception& ex) {
		pisex::ErrorLog("[%s] OnPluginUpdate failed: %s", kLogTag, ex.what());
	} catch (...) {
		pisex::ErrorLog("[%s] OnPluginUpdate failed with an unknown exception", kLogTag);
	}
	return {};
}

plg::PluginResult VipSmokePlugin::OnPluginEnd() {
	try {
		g_pVIPCore->VIP_OnVIPLoadedUnregister();
		if (m_listening && __s2sdk_OnEntitySpawned_Unregister) {
			// The trampoline points into this library, which is about to be unmapped.
			s2sdk::OnEntitySpawned_Unregister(&OnEntitySpawnedListener);
		}
		pisex::Shutdown();
	} catch (...) {
		// nothing left to do at unload
	}
	return {};
}

void VipSmokePlugin::OnCoreReady() {
	if (m_coreReady) {
		return;
	}
	m_coreReady = true;
	if (!m_listening) {
		// The original added its entity listener to the entity system here.
		if (!__s2sdk_OnEntitySpawned_Register) {
			pisex::ErrorLog("[%s] the s2sdk entity spawned listener is unavailable", kLogTag);
			return;
		}
		s2sdk::OnEntitySpawned_Register(&OnEntitySpawnedListener);
		m_listening = true;
	}
}

void VipSmokePlugin::ApplySmokeColor(int iEntityHandle) {
	try {
		// hGrenadeProjectile.Get(): the handle may be dead by now.
		const int iEntity = pisex::EntHandleToEntIndex(iEntityHandle);
		if (iEntity <= 0 || !pisex::IsValidEntity(iEntity)) {
			return;
		}
		// The index may have been recycled; only a smoke projectile is ours.
		if (pisex::GetEntityClassname(iEntity) != kSmokeGrenadeClass) {
			return;
		}

		// The original's thrower lookup: no pawn, or a dead one, ends the work.
		const int iPawn = pisex::GetEntEntity(iEntity, kSmokeGrenadeSchemaClass, "m_hThrower");
		if (iPawn <= 0 || !pisex::IsValidEntity(iPawn)) {
			return;
		}
		if (pisex::GetEntInt(iPawn, "CBaseEntity", "m_lifeState") != kLifeAlive) {
			return;
		}

		// The original's controller -> player slot chain, with the slot bounds check.
		// s2sdk::EntPointerToPlayerSlot calls its binding slot unchecked - guard it (the thrower
		// is still a player without the binding, the colour is just not applied).
		if (!__s2sdk_EntPointerToPlayerSlot) {
			return;
		}
		void* const pPawn = pisex::EntIndexToEntPointer(iPawn);
		const int iSlot = pPawn != nullptr ? s2sdk::EntPointerToPlayerSlot(pPawn) : -1;
		if (iSlot < 0 || iSlot >= kMaxSlots) {
			return;
		}

		// The thrower must be a VIP.
		if (!g_pVIPCore->VIP_IsClientVIP(iSlot)) {
			return;
		}

		// The thrower's "smoke_color" feature value
		// (copied: the API's buffer is reused by the next feature query).
		const std::string sColor = g_pVIPCore->VIP_GetClientFeatureString(iSlot, "smoke_color");
		if (sColor.empty()) {
			return;
		}

		// The value is split and parsed (smoke_color.hpp); the result is applied below.
		vip_smoke::Color color;
		switch (vip_smoke::ParseSmokeColor(sColor, color)) {
		case vip_smoke::ColorParse::Random:
			// A random colour: the original's three random channel values.
			color.red = std::rand() % 255;
			color.green = std::rand() % 255;
			color.blue = std::rand() % 255;
			break;
		case vip_smoke::ColorParse::Invalid:
			pisex::ErrorLog("[%s] Invalid smoke color format: %s", kLogTag, sColor.c_str());
			return;
		case vip_smoke::ColorParse::Color:
			break;
		}
		pisex::SetEntVector(iEntity, kSmokeGrenadeSchemaClass, "m_vSmokeColor",
							plg::vec3{static_cast<float>(color.red), static_cast<float>(color.green),
									  static_cast<float>(color.blue)});
	} catch (const std::exception& ex) {
		pisex::ErrorLog("[%s] ApplySmokeColor failed: %s", kLogTag, ex.what());
	} catch (...) {
		pisex::ErrorLog("[%s] ApplySmokeColor failed with an unknown exception", kLogTag);
	}
}

} // namespace

PLUGIFY_PLUGIN(PLUGIN_API, &g_plugin)
