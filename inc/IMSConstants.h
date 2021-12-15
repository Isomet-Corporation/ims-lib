/*-----------------------------------------------------------------------------
/ Title      : iMS Useful Type Defines Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/IMSConstants.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2020-06-05 07:45:07 +0100 (Fri, 05 Jun 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 443 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#ifndef IMS_IMSCONSTANTS_H__
#define IMS_IMSCONSTANTS_H__

namespace iMS {

	const std::uint16_t ACR_AmplitudeControl_bitmask = 0x000C;
	const std::uint16_t ACR_AmplitudeControlUpper_bitmask = 0x00C0;
	const std::uint16_t ACR_AmplitudeControlCh1_bitmask = 0x0004;
	const std::uint16_t ACR_AmplitudeControlCh2_bitmask = 0x0008;
	const std::uint16_t ACR_AmplitudeControlCh3_bitmask = 0x0040;
	const std::uint16_t ACR_AmplitudeControlCh4_bitmask = 0x0080;
	const std::uint16_t ACR_ClearSTM_bitmask = 0x2000;

	const std::uint16_t ACR_AmplitudeControl_OFF = 0x0000;
	const std::uint16_t ACR_AmplitudeControl_EXTERNAL = 0x0004;
	const std::uint16_t ACR_AmplitudeControl_WIPER_1 = 0x0008;
	const std::uint16_t ACR_AmplitudeControl_WIPER_2 = 0x000C;

	const std::uint16_t ACR_ClearSTM = 0x2000;
	const std::uint16_t ACR_LocalInUse = 0x2000;

	const std::uint16_t SYNTH_REG_IOSig = 1;
	const std::uint16_t SYNTH_REG_IOSig_ANLG_A_Mask = 0x700;
	const std::uint16_t SYNTH_REG_IOSig_ANLG_B_Mask = 0xF800;
	const std::uint16_t SYNTH_REG_IOSig_DIG_Mask = 0xE0;
	const int SYNTH_REG_IOSig_ANLG_A_Shift = 8;
	const int SYNTH_REG_IOSig_ANLG_B_Shift = 11;
	const int SYNTH_REG_IOSig_DIG_Shift = 5;

	const std::uint16_t SYNTH_REG_IO_Signal_Control = 1;
	const std::uint16_t SYNTH_REG_SingleTone_Phase = 2;
	const std::uint16_t SYNTH_REG_SingleTone_Ampl = 3;
	const std::uint16_t SYNTH_REG_SingleTone_Freq = 4;
	const std::uint16_t SYNTH_REG_Phase_Offset_Ch1 = 9;
	const std::uint16_t SYNTH_REG_Phase_Offset_Ch2 = 10;
	const std::uint16_t SYNTH_REG_Phase_Offset_Ch3 = 11;
	const std::uint16_t SYNTH_REG_Phase_Offset_Ch4 = 12;
	const std::uint16_t SYNTH_REG_Pix_Control = 13;
	const std::uint16_t SYNTH_REG_Channel_Swap = 13;
	const std::uint16_t SYNTH_REG_NHF_Timeout = 18;
	const std::uint16_t SYNTH_REG_Clear_NHF = 19;
	const std::uint16_t SYNTH_REG_NHF_Action = 20;
	const std::uint16_t SYNTH_REG_UseLocal = 26;
	const std::uint16_t SYNTH_REG_IO_Config_Mask = 31;
	const std::uint16_t SYNTH_REG_PDI_Checksum = 48;
	const std::uint16_t SYNTH_REG_ENC_Control = 49;
	const std::uint16_t SYNTH_REG_ENC_CoeffKP = 50;
	const std::uint16_t SYNTH_REG_ENC_CoeffKI = 51;
	const std::uint16_t SYNTH_REG_ENC_VelEstX = 52;
	const std::uint16_t SYNTH_REG_ENC_VelEstY = 53;
	const std::uint16_t SYNTH_REG_ENC_VelGainX = 54;
	const std::uint16_t SYNTH_REG_ENC_VelGainY = 55;
	const std::uint16_t SYNTH_REG_SDOR_Pulse = 56;
	const std::uint16_t SYNTH_REG_SDOR_Delay = 57;
	const std::uint16_t SYNTH_REG_STM_FuncHold = 61;
	const std::uint16_t SYNTH_REG_Chan_Scope = 69;
	const std::uint16_t SYNTH_REG_Sync_Update = 70;
	const std::uint16_t SYNTH_REG_Phase_Resync = 71;
	const std::uint16_t SYNTH_REG_Image_Format = 72;
	const std::uint16_t SYNTH_REG_Freq_Offset_Ch1 = 74;
	const std::uint16_t SYNTH_REG_Freq_Offset_Ch2 = 75;
	const std::uint16_t SYNTH_REG_Freq_Offset_Ch3 = 76;
	const std::uint16_t SYNTH_REG_Freq_Offset_Ch4 = 77;
	const std::uint16_t SYNTH_REG_UseLocalIndex = 78;

	const std::uint16_t ACR_RFGate_bitmask = 0x0001;
	const std::uint16_t ACR_EXTEn_bitmask = 0x0002;
	const std::uint16_t ACR_RFBias_bitmask = 0x0030;

	const std::uint16_t ACR_RFGate_ON = 0x0001;
	const std::uint16_t ACR_RFGate_OFF = 0x0000;
	const std::uint16_t ACR_EXTEn_ON = 0x0000;
	const std::uint16_t ACR_EXTEn_OFF = 0x0002;
	const std::uint16_t ACR_RFBias12_OFF = 0x0010;
	const std::uint16_t ACR_RFBias12_ON = 0x0000;
	const std::uint16_t ACR_RFBias34_OFF = 0x0020;
	const std::uint16_t ACR_RFBias34_ON = 0x0000;

	const std::uint16_t ENC_Control_ENC_Mode_Quadrature = 0;
	const std::uint16_t ENC_Control_ENC_Mode_Clk_Dir = 1;
	const std::uint16_t ENC_Control_Vel_Mode_Fast = 0;
	const std::uint16_t ENC_Control_Vel_Mode_Slow = 2;
}

#endif
