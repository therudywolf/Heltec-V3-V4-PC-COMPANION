/*
 * I-Bus / K-Bus device and message IDs (E39/E46).
 */
#ifndef IBUS_DEFINES_H
#define IBUS_DEFINES_H

#define IBUS_GM   0x00
#define IBUS_CDC  0x18
#define IBUS_MFL  0x50
#define IBUS_PDC  0x60
#define IBUS_RAD  0x68
#define IBUS_DSP  0x6A
#define IBUS_IKE  0x80
#define IBUS_MID  0xC0
#define IBUS_TEL  0xC8
#define IBUS_LCM  0xD0
#define IBUS_GLO  0xBF
#define IBUS_DIA  0x3F
#define IBUS_EWS  0x44

#define IBUS_DEV_STAT_REQ    0x01
#define IBUS_DEV_STAT_RDY    0x02
#define IBUS_VEHICLE_CTRL    0x0C
#define IBUS_GM_STAT_REQ     0x79
#define IBUS_GM_STAT_RPLY    0x7A
#define IBUS_IGN_STAT_REQ   0x10
#define IBUS_IGN_STAT_RPLY   0x11
#define IBUS_ODMTR_STAT_REQ  0x16
#define IBUS_ODMTR_STAT_RPLY 0x17
#define IBUS_SPEED_RPM_REQ   0x18
#define IBUS_TEMP            0x19
#define IBUS_IKE_TXT_GONG    0x1A
#define IBUS_UPDATE_MID      0x23
#define IBUS_MFL_BUTTON      0x3B
#define IBUS_CD_CTRL_REQ     0x38
#define IBUS_CD_STAT_RPLY    0x39

#endif
