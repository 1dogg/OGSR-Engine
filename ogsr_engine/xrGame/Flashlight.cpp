#include "StdAfx.h"
#include "Flashlight.h"
#include "ui/ArtefactDetectorUI.h"
#include "HUDManager.h"
#include "Inventory.h"
#include "Level.h"
#include "map_manager.h"
#include "ActorEffector.h"
#include "Actor.h"
#include "player_hud.h"
#include "Weapon.h"
#include "hudsound.h"
#include "ai_sounds.h"
#include "../xr_3da/LightAnimLibrary.h"
#include "../xr_3da/camerabase.h"
#include "../xr_3da/xr_collide_form.h"

ENGINE_API int g_current_renderer;

CFlashlight::CFlashlight()
{
	light_render = ::Render->light_create();
	light_render->set_type(IRender_Light::SPOT);
	light_render->set_shadow(true);
	light_omni = ::Render->light_create();
	light_omni->set_type(IRender_Light::POINT);
	light_omni->set_shadow(false);

	glow_render = ::Render->glow_create();
	lanim = 0;
	fBrightness = 1.f;

	m_light_section = "torch_definition";

	def_lanim = "";
	light_trace_bone = "";
	lightRenderState = false;
	isFlickering = false;
	l_flickerChance = 0;
	l_flickerDelay = 0.0f;
	lastFlicker = 0.0f;

	m_bFastAnimMode = false;
	m_bNeedActivation = false;
}

CFlashlight::~CFlashlight()
{
	TurnDeviceInternal(false);
	light_render.destroy();
	light_omni.destroy();
	glow_render.destroy();
	HUD_SOUND::DestroySound(sndShow);
	HUD_SOUND::DestroySound(sndHide);
	HUD_SOUND::DestroySound(sndTurnOn);
	HUD_SOUND::DestroySound(sndTurnOff);
}

bool CFlashlight::CheckCompatibilityInt(CHudItem* itm, u16* slot_to_activate)
{
	if (itm == nullptr)
		return true;

	CInventoryItem& iitm = itm->item();
	u32 slot = iitm.BaseSlot();
	CActor* pActor = smart_cast<CActor*>(H_Parent());
	auto& Inv = pActor->inventory();
	bool bres = (slot == SECOND_WEAPON_SLOT || slot == KNIFE_SLOT || slot == BOLT_SLOT); // || iitm.AllowDevice();
	if (!bres && slot_to_activate)
	{
		*slot_to_activate = NO_ACTIVE_SLOT;
		if (Inv.ItemFromSlot(BOLT_SLOT))
			*slot_to_activate = BOLT_SLOT;

		if (Inv.ItemFromSlot(KNIFE_SLOT))
			*slot_to_activate = KNIFE_SLOT;

		if (Inv.ItemFromSlot(GRENADE_SLOT) && Inv.ItemFromSlot(GRENADE_SLOT)->BaseSlot() != GRENADE_SLOT)
			*slot_to_activate = GRENADE_SLOT;

		if (Inv.ItemFromSlot(SECOND_WEAPON_SLOT) && Inv.ItemFromSlot(SECOND_WEAPON_SLOT)->BaseSlot() != GRENADE_SLOT)
			*slot_to_activate = SECOND_WEAPON_SLOT;

		if (*slot_to_activate != NO_ACTIVE_SLOT)
			bres = true;
	}

	if (itm->GetState() != CHUDState::eShowing)
		bres = bres && !itm->IsPending();

	if (bres)
	{
		CWeapon* W = smart_cast<CWeapon*>(itm);
		if (W)
			bres = bres &&
			(W->GetState() != CHUDState::eBore) &&
			(W->GetState() != CWeapon::eReload) &&
			(W->GetState() != CWeapon::eSwitch) &&
			(m_bCanBeZoomed || !W->IsZoomed());
	}
	return bres;
}

bool CFlashlight::CheckCompatibility(CHudItem* itm)
{
	if (!inherited::CheckCompatibility(itm))
		return false;

	if (!CheckCompatibilityInt(itm, NULL))
	{
		HideDevice(true);
		return false;
	}
	return true;
}

void CFlashlight::HideDevice(bool bFastMode)
{
	if (GetState() != eHidden)
		ToggleDevice(bFastMode);
}

void CFlashlight::ShowDevice(bool bFastMode)
{
	if (GetState() == eHidden)
		ToggleDevice(bFastMode);
}

void CFlashlight::ForceHide()
{
	g_player_hud->detach_item(this);
}

void CFlashlight::ToggleDevice(bool bFastMode)
{
	m_bNeedActivation = false;
	m_bFastAnimMode = bFastMode;

	if (GetState() == eHidden)
	{
		CActor* pActor = smart_cast<CActor*>(H_Parent());
		auto& Inv = pActor->inventory();
		PIItem iitem = Inv.ActiveItem();
		CHudItem* itm = (iitem) ? iitem->cast_hud_item() : NULL;
		u16 slot_to_activate = NO_ACTIVE_SLOT;

		if (CheckCompatibilityInt(itm, &slot_to_activate))
		{
			if (slot_to_activate != NO_ACTIVE_SLOT)
			{
				Inv.Activate(slot_to_activate);
				m_bNeedActivation = true;
			}
			else
				SwitchState(eShowing);
		}
	}
	else
		SwitchState(eHiding);
}

void CFlashlight::OnStateSwitch(u32 S, u32 oldState)
{
	inherited::OnStateSwitch(S, oldState);

    switch (S)
    {
        case eShowing:
        {
            g_player_hud->attach_item(this);

            TurnDeviceInternal(true);

            HUD_SOUND::PlaySound(sndShow, Fvector().set(0, 0, 0), this, true, false);

            attachable_hud_item* i0 = g_player_hud->attached_item(0);
            if (m_bCanBeZoomed && i0)
            {
                CWeapon* wpn = smart_cast<CWeapon*>(i0->m_parent_hud_item);
                if (wpn && wpn->IsZoomed() && wpn->GetZRotatingFactor() > .5f)
                    m_bZoomed = true;
            }
            m_bZoomed&& AnimationExist("anm_zoom_show")
            ? PlayHUDMotion("anm_zoom_show", FALSE, GetState())
            : PlayHUDMotion(m_bFastAnimMode ? "anm_show_fast" : "anm_show", FALSE, GetState());
            SetPending(TRUE);
        }
            break;
        case eHiding:
        {
            if (oldState != eHiding)
            {
                HUD_SOUND::PlaySound(sndHide, Fvector().set(0, 0, 0), this, true, false);

                m_fZoomfactor > .5f && (oldState == eIdleZoom || oldState == eIdleZoomIn || oldState == eIdleZoomOut) && AnimationExist(m_bFastAnimMode ? "anm_zoom_hide_fast" : "anm_zoom_hide")
                ? PlayHUDMotion(m_bFastAnimMode ? "anm_zoom_hide_fast" : "anm_zoom_hide", TRUE, GetState())
                : PlayHUDMotion(m_bFastAnimMode ? "anm_hide_fast" : "anm_hide", TRUE, GetState());
                SetPending(TRUE);

                m_bZoomed = false;
            }
        }
            break;
        case eIdle:
        {
            m_bZoomed = false;
            PlayAnimIdle();
            SetPending(FALSE);
        }
            break;
        case eIdleZoom:
        {
            m_bZoomed = true;
            PlayHUDMotion("anm_idle_zoom", TRUE, GetState());
            SetPending(FALSE);
        }
            break;
        case eIdleZoomIn:
        {
            m_bZoomed = true;

            if (AnimationExist("anm_zoom")) {
                PlayHUDMotion("anm_zoom", TRUE, GetState());
            }
            else {
                SwitchState(eIdleZoom);
            }

            SetPending(FALSE);
        }
            break;
        case eIdleZoomOut:
        {
            m_bZoomed = false;

            if (AnimationExist("anm_zoom")) {
                PlayHUDMotion("anm_zoom", TRUE, GetState());
            }
            else {
                SwitchState(eIdle);
            }

            SetPending(FALSE);
        }
            break;
        case eIdleThrowStart:
        {
            if (AnimationExist("anm_throw_start"))
            {
                PlayHUDMotion("anm_throw_start", TRUE, GetState());
                SetPending(TRUE);
            }
            else
                SwitchState(eIdleThrow);
        }
            break;
        case eIdleThrow:
        {
            PlayHUDMotion("anm_throw", TRUE, GetState());
            SetPending(FALSE);
        }
            break;
        case eIdleThrowEnd:
        {
            if (AnimationExist("anm_throw_end"))
            {
                PlayHUDMotion("anm_throw_end", TRUE, GetState());
                SetPending(TRUE);
            }
            else
                SwitchState(eIdle);
        }
            break;
    }
}

void CFlashlight::OnAnimationEnd(u32 state)
{
	inherited::OnAnimationEnd(state);
	switch (state)
	{
	case eShowing:
	{
		if (m_bCanBeZoomed)
		{
			attachable_hud_item* i0 = g_player_hud->attached_item(0);
			if (i0)
			{
				CWeapon* wpn = smart_cast<CWeapon*>(i0->m_parent_hud_item);
				if (wpn && wpn->IsZoomed())
				{
					SwitchState(eIdleZoom);
					return;
				}
			}
		}
		SwitchState(eIdle);
	}
	break;
	case eHiding:
	{
		SwitchState(eHidden);
		TurnDeviceInternal(false);
		g_player_hud->detach_item(this);
		m_fZoomfactor = 0.f;
	}
	break;
	case eIdleZoomIn:
	{
		SwitchState(eIdleZoom);
	}
	break;
	case eIdleZoomOut:
	{
		SwitchState(eIdle);
	}
	break;
	case eIdleThrowStart:
	{
		SwitchState(eIdleThrow);
	}
	break;
	case eIdleThrowEnd:
	{
		SwitchState(eIdle);
	}
	break;
	}
}

void CFlashlight::UpdateXForm()
{
	CInventoryItem::UpdateXForm();
}

void CFlashlight::OnActiveItem()
{
	return;
}

void CFlashlight::OnHiddenItem()
{
}

BOOL CFlashlight::net_Spawn(CSE_Abstract* DC)
{
	TurnDeviceInternal(false);

	if (!inherited::net_Spawn(DC))
		return FALSE;

	bool b_r2 = !!psDeviceFlags.test(rsR2);
	b_r2 |= !!psDeviceFlags.test(rsR3);
	b_r2 |= !!psDeviceFlags.test(rsR4);

	def_lanim = pSettings->r_string(m_light_section, "color_animator");
	lanim = LALib.FindItem(def_lanim);

	Fcolor clr = pSettings->r_fcolor(m_light_section, (b_r2) ? "color_r2" : "color");
	def_color = clr;
	fBrightness = clr.intensity();
	float range = pSettings->r_float(m_light_section, (b_r2) ? "range_r2" : "range");
	light_render->set_color(clr);
	light_render->set_range(range);
	light_render->set_hud_mode(true);

	Fcolor clr_o = pSettings->r_fcolor(m_light_section, (b_r2) ? "omni_color_r2" : "omni_color");
	float range_o = pSettings->r_float(m_light_section, (b_r2) ? "omni_range_r2" : "omni_range");
	light_omni->set_color(clr_o);
	light_omni->set_range(range_o);
	light_omni->set_hud_mode(true);

	light_render->set_cone(deg2rad(pSettings->r_float(m_light_section, "spot_angle")));
	light_render->set_texture(READ_IF_EXISTS(pSettings, r_string, m_light_section, "spot_texture", (0)));

	glow_render->set_texture(pSettings->r_string(m_light_section, "glow_texture"));
	glow_render->set_color(clr);
	glow_render->set_radius(pSettings->r_float(m_light_section, "glow_radius"));

	light_render->set_volumetric(!!READ_IF_EXISTS(pSettings, r_bool, m_light_section, "volumetric", 0));
	light_render->
		set_volumetric_quality(READ_IF_EXISTS(pSettings, r_float, m_light_section, "volumetric_quality", 1.f));
	light_render->set_volumetric_intensity(READ_IF_EXISTS(pSettings, r_float, m_light_section, "volumetric_intensity",
		1.f));
	light_render->set_volumetric_distance(READ_IF_EXISTS(pSettings, r_float, m_light_section, "volumetric_distance",
		1.f));
	light_render->set_type((IRender_Light::LT)(READ_IF_EXISTS(pSettings, r_u8, m_light_section, "type", 2)));
	light_omni->set_type((IRender_Light::LT)(READ_IF_EXISTS(pSettings, r_u8, m_light_section, "omni_type", 1)));

	return TRUE;
}

void CFlashlight::Load(LPCSTR section)
{
	inherited::Load(section);

	HUD_SOUND::LoadSound(section, "snd_draw", sndShow, SOUND_TYPE_ITEM_TAKING);
	HUD_SOUND::LoadSound(section, "snd_holster", sndHide, SOUND_TYPE_ITEM_HIDING);
	HUD_SOUND::LoadSound(section, "snd_draw", sndTurnOn, SOUND_TYPE_ITEM_USING);
	HUD_SOUND::LoadSound(section, "snd_holster", sndTurnOff, SOUND_TYPE_ITEM_USING);

	m_bCanBeZoomed = pSettings->line_exist(pSettings->r_string(section, "hud"), "anm_zoom");
	m_bThrowAnm = pSettings->line_exist(pSettings->r_string(section, "hud"), "anm_throw");

	light_trace_bone = READ_IF_EXISTS(pSettings, r_string, section, "light_trace_bone", "");
	m_light_section = READ_IF_EXISTS(pSettings, r_string, section, "light_section", "torch_definition");
}

void CFlashlight::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);

	if (!IsWorking()) return;

	Position().set(H_Parent()->Position());
}

bool CFlashlight::IsWorking()
{
	return m_bWorking && H_Parent() && H_Parent() == Level().CurrentViewEntity();
}

void CFlashlight::UpdateVisibility()
{
	//check visibility
	attachable_hud_item* i0 = g_player_hud->attached_item(0);
	if (i0 && HudItemData())
	{
		bool bClimb = ((Actor()->MovingState() & mcClimb) != 0);
		if (bClimb)
		{
			HideDevice(true);
			m_bNeedActivation = true;
		}
		else
		{
			CInventoryItem* itm = g_actor->inventory().ActiveItem();
			CWeapon* wpn = smart_cast<CWeapon*>(itm);
			if (GetState() == eIdleThrowStart || GetState() == eIdleThrow)
				SwitchState(eIdle);

			if (wpn)
			{
				u32 state = wpn->GetState();
				if ((wpn->IsZoomed() && !m_bCanBeZoomed) || state == CWeapon::eReload || state == CWeapon::eSwitch)
				{
					HideDevice(true);
					m_bNeedActivation = true;
					return;
				}
				else if (wpn->IsZoomed())
				{
					if (GetState() == eIdle || GetState() == eIdleZoomOut || GetState() == eShowing)
						SwitchState(eIdleZoomIn);
				}
				else if (GetState() == eIdleZoom || GetState() == eIdleZoomIn)
				{
					SwitchState(eIdleZoomOut);
				}

				// Sync zoom in/out anim to zoomfactor :)
				if (GetState() == eIdleZoomIn || GetState() == eIdleZoomOut)
				{
					if (m_fZoomfactor > 0.f && m_fZoomfactor < 1.f)
						i0->m_parent->set_part_cycle_time(1, m_fZoomfactor);
					else if (m_fZoomfactor == 0)
					{
						PlayHUDMotion("anm_idle", FALSE, eIdle); // Necessary to fix small jump caused by motion mixing
						SwitchState(eIdle);
					}
				}
			}
		}
	}
	else
	{
		if (GetState() == eIdleZoom)
			SwitchState(eIdleZoomOut);

		if (GetState() == eIdleZoomIn)
			SwitchState(eIdle);

		if (m_bNeedActivation)
		{
			attachable_hud_item* i0 = g_player_hud->attached_item(0);
			bool bClimb = ((Actor()->MovingState() & mcClimb) != 0);
			if (!bClimb)
			{
				CHudItem* huditem = (i0) ? i0->m_parent_hud_item : NULL;
				bool bChecked = !huditem || CheckCompatibilityInt(huditem, 0);

				if (bChecked)
					ShowDevice(true);
			}
		}
	}
}

void CFlashlight::UpdateWork()
{
}

void CFlashlight::TurnDeviceInternal(bool b)
{
	m_bWorking = b;
	if (can_use_dynamic_lights())
	{
		light_render->set_active(b);
		light_omni->set_active(b);
	}
	glow_render->set_active(b);
}

bool CFlashlight::torch_active() const
{
	return m_bWorking;
}

void CFlashlight::SwitchLightOnly()
{
	lightRenderState = !lightRenderState;

	if (can_use_dynamic_lights())
	{
		light_render->set_active(lightRenderState);
		light_omni->set_active(lightRenderState);
	}

	glow_render->set_active(lightRenderState);
}

void CFlashlight::UpdateCL()
{
	inherited::UpdateCL();

	CActor* actor = smart_cast<CActor*>(H_Parent());
	if (!actor)
		return;

	if (!IsWorking())
		return;

	if (!HudItemData())
	{
		TurnDeviceInternal(false);
		return;
	}

	firedeps dep;
	HudItemData()->setup_firedeps(dep);

	light_render->set_position(dep.vLastFP);
	light_omni->set_position(dep.vLastFP);
	glow_render->set_position(dep.vLastFP);

	Fvector dir = dep.m_FireParticlesXForm.k;

	/*if (Actor()->cam_freelook != eflDisabled)
	{
		dir.setHP(-angle_normalize_signed(Actor()->old_torso_yaw), dir.getP() > 0.f ? dir.getP() * .6f : dir.getP() * .8f);
		dir.lerp(dep.m_FireParticlesXForm.k, dir, Actor()->freelook_cam_control);
	}*/

	light_render->set_rotation(dir, dep.m_FireParticlesXForm.i);
	light_omni->set_rotation(dir, dep.m_FireParticlesXForm.i);
	glow_render->set_direction(dir);

	// Update flickering
	if (isFlickering)
	{
		float tg = Device.fTimeGlobal;
		if (lastFlicker == 0) lastFlicker = tg;
		if (tg - lastFlicker >= l_flickerDelay)
		{
			int rando = rand() % 100 + 1;
			if (rando >= l_flickerChance)
			{
				SwitchLightOnly();
			}
			lastFlicker = tg;
		}
	}

	if (!lanim)
		return;

	int frame;

	u32 clr = lanim->CalculateBGR(Device.fTimeGlobal, frame);

	Fcolor fclr;
	fclr.set((float)color_get_B(clr), (float)color_get_G(clr), (float)color_get_R(clr), 1.f);
	fclr.mul_rgb(fBrightness / 255.f);
	if (can_use_dynamic_lights())
	{
		light_render->set_color(fclr);
		light_omni->set_color(fclr);
	}
	glow_render->set_color(fclr);
}

void CFlashlight::SetLanim(LPCSTR name, bool bFlicker, int flickerChance, float flickerDelay, float framerate)
{
	lanim = LALib.FindItem(name);
	if (lanim && framerate)
		lanim->SetFramerate(framerate);
	isFlickering = bFlicker;
	if (isFlickering)
	{
		l_flickerChance = flickerChance;
		l_flickerDelay = flickerDelay;
	}
}

void CFlashlight::ResetLanim()
{
	if (lanim)
	{
		lanim->ResetFramerate();
		if (lanim->cName != def_lanim)
			lanim = LALib.FindItem(def_lanim);
	}

	if (can_use_dynamic_lights())
	{
		light_render->set_color(def_color);
		light_omni->set_color(def_color);
	}
	glow_render->set_color(def_color);

	isFlickering = false;
}

inline bool CFlashlight::can_use_dynamic_lights()
{
	if (!H_Parent())
		return (true);

	CInventoryOwner* owner = smart_cast<CInventoryOwner*>(H_Parent());
	if (!owner)
		return (true);

	return (owner->can_use_dynamic_lights());
}

void CFlashlight::OnH_B_Independent(bool just_before_destroy)
{
	inherited::OnH_B_Independent(just_before_destroy);

	if (GetState() != eHidden)
	{
		// Detaching hud item and animation stop in OnH_A_Independent
		TurnDeviceInternal(false);
		SwitchState(eHidden);
	}
}

void CFlashlight::OnMoveToRuck(EItemPlace prevPlace)
{
	inherited::OnMoveToRuck(prevPlace);
	if (prevPlace == eItemPlaceSlot)
	{
		SwitchState(eHidden);
		g_player_hud->detach_item(this);
	}
	TurnDeviceInternal(false);
	StopCurrentAnimWithoutCallback();
}

void CFlashlight::on_a_hud_attach()
{
	inherited::on_a_hud_attach();
}