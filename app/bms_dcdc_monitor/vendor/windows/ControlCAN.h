#ifndef CONTROLCAN_H
#define CONTROLCAN_H

#include <windows.h>

#define VCI_USBCAN1 3
#define VCI_USBCAN2 4
#define VCI_USBCAN2A 4
#define VCI_USBCAN_E_U 20
#define VCI_USBCAN_2E_U 21

#define STATUS_OK 1
#define STATUS_ERR 0

typedef struct _VCI_BOARD_INFO
{
    USHORT hw_Version;
    USHORT fw_Version;
    USHORT dr_Version;
    USHORT in_Version;
    USHORT irq_Num;
    BYTE can_Num;
    CHAR str_Serial_Num[20];
    CHAR str_hw_Type[40];
    USHORT Reserved[4];
} VCI_BOARD_INFO, *PVCI_BOARD_INFO;

typedef struct _VCI_CAN_OBJ
{
    UINT ID;
    UINT TimeStamp;
    BYTE TimeFlag;
    BYTE SendType;
    BYTE RemoteFlag;
    BYTE ExternFlag;
    BYTE DataLen;
    BYTE Data[8];
    BYTE Reserved[3];
} VCI_CAN_OBJ, *PVCI_CAN_OBJ;

typedef struct _VCI_INIT_CONFIG
{
    DWORD AccCode;
    DWORD AccMask;
    DWORD Reserved;
    UCHAR Filter;
    UCHAR Timing0;
    UCHAR Timing1;
    UCHAR Mode;
} VCI_INIT_CONFIG, *PVCI_INIT_CONFIG;

#endif // CONTROLCAN_H
