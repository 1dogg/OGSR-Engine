#pragma once
#include "hud_item_object.h"
#include "HudSound.h"
#include "ai_sounds.h"

class CLAItem;

class CFlashlight : public CHudItemObject {
private:
	typedef	CHudItemObject	inherited;
	inline	bool	can_use_dynamic_lights();

protected:
	bool			m_bFastAnimMode;
	bool			m_bNeedActivation;

	LPCSTR def_lanim;
	Fcolor def_color;

	bool lightRenderState;
	bool isFlickering;
	int l_flickerChance;
	float l_flickerDelay;
	float lastFlicker;

	bool m_bCanBeZoomed;
	bool m_bThrowAnm;
	bool m_bWorking;

	float			fBrightness;
	CLAItem* lanim;

	shared_str		light_trace_bone;

	ref_light		light_render;
	ref_light		light_omni;
	ref_glow		glow_render;
	shared_str		m_light_section;

	bool			CheckCompatibilityInt(CHudItem* itm, u16* slot_to_activate);
	void			UpdateVisibility();
	void			TurnDeviceInternal(bool b);
	void			UpdateWork();

public:
	CFlashlight();
	virtual ~CFlashlight();

	virtual BOOL net_Spawn(CSE_Abstract* DC);
	virtual void Load(LPCSTR section);

	virtual void OnH_B_Independent(bool just_before_destroy);

	virtual void shedule_Update(u32 dt);
	virtual void UpdateCL();

	enum EFlashlightStates
	{
		eIdleZoom = 5,
		eIdleZoomIn,
		eIdleZoomOut,
		eIdleThrowStart,
		eIdleThrow,
		eIdleThrowEnd
	};

	bool IsWorking();

	virtual void OnMoveToRuck(EItemPlace prevPlace);
	void on_a_hud_attach();

	virtual void OnActiveItem();
	virtual void OnHiddenItem();
	virtual void OnStateSwitch(u32 S, u32 oldState);
	virtual void OnAnimationEnd(u32 state);
	virtual void UpdateXForm();

	virtual void ToggleDevice(bool bFastMode);
	virtual void HideDevice(bool bFastMode);
	virtual void ShowDevice(bool bFastMode);
	virtual void ForceHide();
	virtual bool CheckCompatibility(CHudItem* itm);
	void			SwitchLightOnly();

	virtual CFlashlight* cast_flashlight() { return this; }
	void SetLanim(LPCSTR name, bool bFlicker, int flickerChance, float flickerDelay, float framerate);
	void ResetLanim();
	bool torch_active() const;

	bool m_bZoomed;
	float m_fZoomfactor;

	HUD_SOUND sndShow, sndHide, sndTurnOn, sndTurnOff;
};