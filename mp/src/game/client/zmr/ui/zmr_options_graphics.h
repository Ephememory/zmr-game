#pragma once


#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>

#include "zmr_options_tab.h"


class CZMOptionsSubGraphics : public CZMOptionsSub
{
public:
    DECLARE_CLASS_SIMPLE( CZMOptionsSubGraphics, CZMOptionsSub );

    CZMOptionsSubGraphics( vgui::Panel* parent );
    ~CZMOptionsSubGraphics();

    virtual void OnApplyChanges() OVERRIDE;
    virtual void OnResetData() OVERRIDE;


    MESSAGE_FUNC_PTR( OnSliderMoved, "SliderMoved", panel );
    MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

private:
    vgui::CheckButton*  m_pCheck_WepGlow;
    vgui::CheckButton*  m_pCheck_AmmoGlow;
    vgui::CheckButton*  m_pCheck_VisionDynLight;
    vgui::CheckButton*  m_pCheck_SilhouetteVision;
    vgui::Slider*       m_pSlider_MaxRagdolls;
    vgui::TextEntry*    m_pTextEntry_MaxRagdolls;
    vgui::ComboBox*     m_pRainBox;
    vgui::ComboBox*     m_pExpFlashlightAmtBox;
    vgui::CheckButton*  m_pCheck_CC;
    vgui::ComboBox*     m_pMuzzleflashBox;
    vgui::CheckButton*  m_pCheck_CinematicBars;
    vgui::ComboBox*     m_pFireLightsBox;
};
