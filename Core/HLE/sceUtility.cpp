// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "HLE.h"
#include "../MIPS/MIPS.h"

#include "sceKernel.h"
#include "sceKernelThread.h"
#include "sceUtility.h"

#include "sceCtrl.h"
#include "../Util/PPGeDraw.h"
#include "../../native/file/file_util.h"

enum SceUtilitySavedataType
{
	SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD		= 0,
	SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE		= 1,
	SCE_UTILITY_SAVEDATA_TYPE_LOAD			= 2,
	SCE_UTILITY_SAVEDATA_TYPE_SAVE			= 3,
	SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD		= 4,
	SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE		= 5,
	SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE	= 6,
	SCE_UTILITY_SAVEDATA_TYPE_DELETE		= 7,
	SCE_UTILITY_SAVEDATA_TYPE_SIZES			= 8	
} ;

#define SCE_UTILITY_SAVEDATA_ERROR_TYPE					(0x80110300)

#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_NO_MS			(0x80110301)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_EJECT_MS		(0x80110302)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_ACCESS_ERROR	(0x80110305)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_DATA_BROKEN		(0x80110306)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_NO_DATA			(0x80110307)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_PARAM			(0x80110308)
#define SCE_UTILITY_SAVEDATA_ERROR_LOAD_INTERNAL		(0x8011030b)

#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_NO_MS			(0x80110381)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_EJECT_MS		(0x80110382)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_MS_NOSPACE		(0x80110383)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_MS_PROTECTED	(0x80110384)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_ACCESS_ERROR	(0x80110385)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_PARAM			(0x80110388)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_NO_UMD			(0x80110389)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_WRONG_UMD		(0x8011038a)
#define SCE_UTILITY_SAVEDATA_ERROR_SAVE_INTERNAL		(0x8011038b)

#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_NO_MS			(0x80110341)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_EJECT_MS		(0x80110342)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_MS_PROTECTED	(0x80110344)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_ACCESS_ERROR	(0x80110345)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_NO_DATA		(0x80110347)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_PARAM			(0x80110348)
#define SCE_UTILITY_SAVEDATA_ERROR_DELETE_INTERNAL		(0x8011034b)

#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_NO_MS			(0x801103C1)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_EJECT_MS		(0x801103C2)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_ACCESS_ERROR	(0x801103C5)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_NO_DATA		(0x801103C7)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_PARAM			(0x801103C8)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_NO_UMD			(0x801103C9)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_WRONG_UMD		(0x801103Ca)
#define SCE_UTILITY_SAVEDATA_ERROR_SIZES_INTERNAL		(0x801103Cb)

#define SCE_UTILITY_STATUS_NONE 		0
#define SCE_UTILITY_STATUS_INITIALIZE	1
#define SCE_UTILITY_STATUS_RUNNING 		2
#define SCE_UTILITY_STATUS_FINISHED 	3
#define SCE_UTILITY_STATUS_SHUTDOWN 	4


// title, savedataTitle, detail: parts of the unencrypted SFO
// data, it contains what the VSH and standard load screen shows
struct PspUtilitySavedataSFOParam
{
	char title[0x80];
	char savedataTitle[0x80];
	char detail[0x400];
	unsigned char parentalLevel;
	unsigned char unknown[3];
};

struct PspUtilitySavedataFileData {
	int buf;
	SceSize bufSize;  // Size of the buffer pointed to by buf
	SceSize size;	    // Actual file size to write / was read
	int unknown;
};

// Structure to hold the parameters for the sceUtilitySavedataInitStart function.
struct SceUtilitySavedataParam
{
	SceSize size; // Size of the structure

	int language;

	int buttonSwap;

	int unknown[4];
	int result;
	int unknown2[4];

	int mode;  // 0 to load, 1 to save
	int bind;

	int overwriteMode;   // use 0x10  ?

	/** gameName: name used from the game for saves, equal for all saves */
	char gameName[13];
	char unused[3];
	/** saveName: name of the particular save, normally a number */
	char saveName[20];
	int saveNameList;
	/** fileName: name of the data file of the game for example DATA.BIN */
	char fileName[13];
	char unused2[3];

	/** pointer to a buffer that will contain data file unencrypted data */
	int dataBuf; // Initially void*, but void* in 64bit system take 8 bytes.
	/** size of allocated space to dataBuf */
	SceSize dataBufSize;
	SceSize dataSize;  // Size of the actual save data

	PspUtilitySavedataSFOParam sfoParam;

	PspUtilitySavedataFileData icon0FileData;
	PspUtilitySavedataFileData icon1FileData;
	PspUtilitySavedataFileData pic1FileData;
	PspUtilitySavedataFileData snd0FileData;

	unsigned char unknown17[4];
};

struct SaveFileInfo
{
	int size;
};

struct SaveDialogInfo
{
	int paramAddr;
	int menuListSelection;
	char (*saveNameListData)[20];
	SaveFileInfo (*saveDataList);
	int saveNameListDataCount;
};


static u32 utilityDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
static u32 saveDataDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
static u32 oskDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
static u32 msgDialogState = SCE_UTILITY_STATUS_SHUTDOWN;


u32 messageDialogAddr;

SaveDialogInfo saveDialogInfo;
std::string icon0Name = "ICON0.PNG";
std::string icon1Name = "ICON1.PNG";
std::string pic1Name = "PIC1.PNG";
std::string sfoName = "PARAM.SFO";

// Save system only coded for PC
#ifdef ANDROID
#elif BLACKBERRY
#else // PC

#define USE_SAVESYSTEM
std::string savePath = "./SaveData";

#endif

void __UtilityInit()
{
	messageDialogAddr = 0;
	utilityDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	saveDataDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	oskDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	msgDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	// Creates a directory for save on the sdcard or MemStick directory
	
	saveDialogInfo.paramAddr = 0;
	saveDialogInfo.saveDataList = 0;
	saveDialogInfo.menuListSelection = 0;
	
#ifdef USE_SAVESYSTEM
	if(!exists(savePath))
	{
		mkDir(savePath);
	}
#endif
	
}


void __UtilityInitStart()
{
	utilityDialogState = SCE_UTILITY_STATUS_INITIALIZE;
}

void __UtilityShutdownStart()
{
	utilityDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
}

void __UtilityUpdate()
{
	if (utilityDialogState == SCE_UTILITY_STATUS_INITIALIZE)
	{
		utilityDialogState = SCE_UTILITY_STATUS_RUNNING;
	}
	else if (utilityDialogState == SCE_UTILITY_STATUS_RUNNING)
	{
		utilityDialogState = SCE_UTILITY_STATUS_FINISHED;
	}
	else if (utilityDialogState == SCE_UTILITY_STATUS_FINISHED)
	{
		utilityDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	}
}

u32 __UtilityGetStatus()
{
	u32 ret = utilityDialogState;
	if (utilityDialogState == SCE_UTILITY_STATUS_SHUTDOWN)
		utilityDialogState = SCE_UTILITY_STATUS_NONE;
	if (utilityDialogState == SCE_UTILITY_STATUS_INITIALIZE)
		utilityDialogState = SCE_UTILITY_STATUS_RUNNING;
	return ret;
}


int sceUtilitySavedataInitStart(u32 paramAddr)
{
	//SceUtilitySavedataParam *param = new SceUtilitySavedataParam();
	saveDialogInfo.paramAddr = paramAddr;
	SceUtilitySavedataParam *param = (SceUtilitySavedataParam*)Memory::GetPointer(saveDialogInfo.paramAddr);
	//saveDialogInfo.menuListSelection = 0; // Keep the one selected by user for autosave
	
	//memcpy(param,data,sizeof(SceUtilitySavedataParam));
	
	if(param->saveNameList != 0)
	{
		int count = 0;
		char str[20];
		u8* data2 = (u8*)Memory::GetPointer(param->saveNameList);
		do
		{
			char* nameData = (char*)(*((u32*)data2));
			memcpy(str,data2,20); data2 += 20;
			count++;
		} while(str[0] != 0);
		count--;
		
		if(saveDialogInfo.saveNameListData)
			delete[] saveDialogInfo.saveNameListData; 
		if(saveDialogInfo.saveDataList)
			delete[] saveDialogInfo.saveDataList;
		saveDialogInfo.saveNameListData = new char[count][20];
		saveDialogInfo.saveDataList = new SaveFileInfo[count];
		saveDialogInfo.saveNameListDataCount = count;
		
		data2 = (u8*)Memory::GetPointer(param->saveNameList);
		for(int i = 0; i <count; i++)
		{
			char* nameData = (char*)*((u32*)data2);
			memcpy(saveDialogInfo.saveNameListData[i],data2,20); data2 += 20;
			DEBUG_LOG(HLE,"Name : %s",saveDialogInfo.saveNameListData[i]);
			
			std::string fileDataPath = savePath+"/"+param->gameName+saveDialogInfo.saveNameListData[i]+"/"+param->fileName;
			if(exists(fileDataPath))
			{
				//FileInfo info;
				//getFileInfo(fileDataPath,&info);
				saveDialogInfo.saveDataList[i].size = 1;
				DEBUG_LOG(HLE,"%s Exist",fileDataPath.c_str());
			}
			else
			{
				saveDialogInfo.saveDataList[i].size = 0;
				DEBUG_LOG(HLE,"Don't Exist");
			}
		}
	}
	

	DEBUG_LOG(HLE,"sceUtilitySavedataInitStart(%08x)", paramAddr);
	DEBUG_LOG(HLE,"Mode: %i", param->mode);
	messageDialogAddr = *((u32*)&param);
	switch(param->mode)
	{
		case SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD: //load
		case SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD:
		case SCE_UTILITY_SAVEDATA_TYPE_LOAD:
		{
			DEBUG_LOG(HLE, "Loading. Title: %s Save: %s File: %s", param->gameName, param->saveName, param->fileName);
		}
		break;
		case SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE:
		case SCE_UTILITY_SAVEDATA_TYPE_SAVE:
		case SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE:
		{
			//save
			DEBUG_LOG(HLE, "Saving. Title: %s Save: %s File: %s", param->gameName, param->saveName, param->fileName);
			//return 0;
		}
		break;
		case SCE_UTILITY_SAVEDATA_TYPE_LISTDELETE:
		case SCE_UTILITY_SAVEDATA_TYPE_DELETE:
		case SCE_UTILITY_SAVEDATA_TYPE_SIZES:
		default: 
		{
			ERROR_LOG(HLE, "Load/Save function %d not coded. Title: %s Save: %s File: %s", param->mode, param->gameName, param->saveName, param->fileName);
			return 0; // Return 0 should allow the game to continue, but missing function must be implemented and returning the right value or the game can block.
		}
		break;
	}

	//__UtilityInitStart();
	saveDataDialogState = SCE_UTILITY_STATUS_INITIALIZE;


	/*INFO_LOG(HLE,"Dump Param :");
	INFO_LOG(HLE,"size : %d",param->size);
	INFO_LOG(HLE,"language : %d",param->language);
	INFO_LOG(HLE,"buttonSwap : %d",param->buttonSwap);
	INFO_LOG(HLE,"result : %d",param->result);
	INFO_LOG(HLE,"mode : %d",param->mode);
	INFO_LOG(HLE,"bind : %d",param->bind);
	INFO_LOG(HLE,"overwriteMode : %d",param->overwriteMode);
	INFO_LOG(HLE,"gameName : %s",param->gameName);
	INFO_LOG(HLE,"saveName : %s",param->saveName);
	INFO_LOG(HLE,"saveNameList : %08x",*((unsigned int*)&param->saveNameList));
	INFO_LOG(HLE,"fileName : %s",param->fileName);
	INFO_LOG(HLE,"dataBuf : %08x",*((unsigned int*)&param->dataBuf));
	INFO_LOG(HLE,"dataBufSize : %u",param->dataBufSize);
	INFO_LOG(HLE,"dataSize : %u",param->dataSize);
	
	INFO_LOG(HLE,"sfo title : %s",param->sfoParam.title);
	INFO_LOG(HLE,"sfo savedataTitle : %s",param->sfoParam.savedataTitle);
	INFO_LOG(HLE,"sfo detail : %s",param->sfoParam.detail);

	INFO_LOG(HLE,"icon0 data : %08x",*((unsigned int*)&param->icon0FileData.buf));
	INFO_LOG(HLE,"icon0 size : %u",param->icon0FileData.bufSize);

	INFO_LOG(HLE,"icon1 data : %08x",*((unsigned int*)&param->icon1FileData.buf));
	INFO_LOG(HLE,"icon1 size : %u",param->icon1FileData.bufSize);

	INFO_LOG(HLE,"pic1 data : %08x",*((unsigned int*)&param->pic1FileData.buf));
	INFO_LOG(HLE,"pic1 size : %u",param->pic1FileData.bufSize);

	INFO_LOG(HLE,"snd0 data : %08x",*((unsigned int*)&param->snd0FileData.buf));
	INFO_LOG(HLE,"snd0 size : %u",param->snd0FileData.bufSize);*/
	
	return 0;
}

int sceUtilitySavedataShutdownStart()
{
	DEBUG_LOG(HLE,"sceUtilitySavedataShutdownStart()");
	//__UtilityShutdownStart();
	saveDataDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	
	saveDialogInfo.paramAddr = 0;
	return 0;
}

int sceUtilitySavedataGetStatus()
{
	if (saveDataDialogState == SCE_UTILITY_STATUS_SHUTDOWN)
		saveDataDialogState = SCE_UTILITY_STATUS_NONE;
	if (saveDataDialogState == SCE_UTILITY_STATUS_INITIALIZE)
		saveDataDialogState = SCE_UTILITY_STATUS_RUNNING;
	u32 retval = saveDataDialogState;
	DEBUG_LOG(HLE,"%i=sceUtilitySavedataGetStatus()", retval);
	return retval;
}

bool saveSaveData()
{
#ifdef USE_SAVESYSTEM
	if (!saveDialogInfo.paramAddr) {
		return false;
	}

	SceUtilitySavedataParam *param = (SceUtilitySavedataParam*)Memory::GetPointer(saveDialogInfo.paramAddr);
	u8* data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->dataBuf));
	
	std::string dirPath = std::string(param->gameName)+param->saveName;
	if(saveDialogInfo.saveNameListDataCount > 0) // if user selection, use it
		dirPath = std::string(param->gameName)+saveDialogInfo.saveNameListData[saveDialogInfo.menuListSelection];
	if(!exists(savePath+"/"+dirPath))
		mkDir(savePath+"/"+dirPath);
	std::string filePath = savePath+"/"+dirPath+"/"+param->fileName;
	INFO_LOG(HLE,"Saving file with size %u in %s",param->dataBufSize,filePath.c_str());
	if(!writeDataToFile(false, data_, param->dataBufSize ,filePath.c_str()))
	{
		ERROR_LOG(HLE,"Error writing file %s",filePath.c_str());
		return false;
	}
	else
	{
	
		// TODO SAVE SFO
		/*data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->dataBuf));
		writeDataToFile(false, );*/
		
		// SAVE ICON0
		if(param->icon0FileData.buf)
		{
			data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->icon0FileData.buf));
			std::string icon0path = savePath+"/"+dirPath+"/"+icon0Name;
			writeDataToFile(false, data_, param->icon0FileData.bufSize, icon0path.c_str());
		}
		// SAVE ICON1
		if(param->icon1FileData.buf)
		{
			data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->icon1FileData.buf));
			std::string icon1path = savePath+"/"+dirPath+"/"+icon1Name;
			writeDataToFile(false, data_, param->icon1FileData.bufSize, icon1path.c_str());
		}
		// SAVE PIC1
		if(param->pic1FileData.buf)
		{
			data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->pic1FileData.buf));
			std::string pic1path = savePath+"/"+dirPath+"/"+pic1Name;
			writeDataToFile(false, data_, param->pic1FileData.bufSize, pic1path.c_str());
		}
	}
	return true;
#else
	return false;
#endif
}

bool loadSaveData()
{
#ifdef USE_SAVESYSTEM
	if (!saveDialogInfo.paramAddr) {
		return false;
	}
	SceUtilitySavedataParam *param = (SceUtilitySavedataParam*)Memory::GetPointer(saveDialogInfo.paramAddr);

	
	u8* data_ = (u8*)Memory::GetPointer(*((unsigned int*)&param->dataBuf));
	
	std::string dirPath = std::string(param->gameName)+param->saveName;
	if(saveDialogInfo.saveNameListDataCount > 0) // if user selection, use it
	{
		dirPath = std::string(param->gameName)+saveDialogInfo.saveNameListData[saveDialogInfo.menuListSelection];
		if(saveDialogInfo.saveDataList[saveDialogInfo.menuListSelection].size == 0) // don't read no existing file
		{
			return false;
		}
	}
	std::string filePath = savePath+"/"+dirPath+"/"+param->fileName;
	INFO_LOG(HLE,"Loading file with size %u in %s",param->dataBufSize,filePath.c_str());
	if(!readDataFromFile(false, data_, param->dataBufSize ,filePath.c_str()))
	{
		ERROR_LOG(HLE,"Error reading file %s",filePath.c_str());
		return false;
	}
	return true;
#else
	return false;
#endif
}

void sceUtilitySavedataUpdate(u32 unknown)
{
	DEBUG_LOG(HLE,"sceUtilitySavedataUpdate()");
	//draw savedata UI here
	
	switch (saveDataDialogState) {
	case SCE_UTILITY_STATUS_FINISHED:
		saveDataDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
		break;
	}

	if (saveDataDialogState != SCE_UTILITY_STATUS_RUNNING)
	{
		//RETURN(0);
		return;
	}
	if (!saveDialogInfo.paramAddr) {
		saveDataDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
		return;
	}


	SceUtilitySavedataParam *param = (SceUtilitySavedataParam*)Memory::GetPointer(saveDialogInfo.paramAddr);

	PPGeBegin();

	PPGeDraw4Patch(I_BUTTON, 0, 0, 480, 272, 0xcFFFFFFF);
	switch(param->mode)
	{
		case SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE:
			PPGeDrawText("Save List", 50, 50, PPGE_ALIGN_LEFT, 0.5f, 0xFFFFFFFF);
			
			for(int i = 0; i < saveDialogInfo.saveNameListDataCount; i++)
			{
				if(i == saveDialogInfo.menuListSelection)
					PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0xFF0000FF);
				else
				{
					if(saveDialogInfo.saveDataList[i].size > 0)
						PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0xFFFFFFFF);
					else
						PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0x888888FF);
				}
			}
			
		break;
		case SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD:
			PPGeDrawText("Load List", 50, 50, PPGE_ALIGN_LEFT, 0.5f, 0xFFFFFFFF);
			
			for(int i = 0; i < saveDialogInfo.saveNameListDataCount; i++)
			{
				if(i == saveDialogInfo.menuListSelection)
					PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0xFF0000FF);
				else
				{
					if(saveDialogInfo.saveDataList[i].size > 0)
						PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0xFFFFFFFF);
					else
						PPGeDrawText(saveDialogInfo.saveNameListData[i], 70, 70+15*i, PPGE_ALIGN_LEFT, 0.5f, 0x888888FF);
				}
			}
		break;
		case SCE_UTILITY_SAVEDATA_TYPE_LOAD: // Only load and exit
		case SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD:
			loadSaveData();
			saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
			PPGeEnd();
			return;
		case SCE_UTILITY_SAVEDATA_TYPE_SAVE: // Only save and exit
		case SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE:
			saveSaveData();
			saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
			PPGeEnd();
			return;
		default:
			saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
			PPGeEnd();
			return;
		break;
	}

	static u32 lastButtons = 0;
	u32 buttons = __CtrlPeekButtons();

	if(param->mode != SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD || saveDialogInfo.saveDataList[saveDialogInfo.menuListSelection].size > 0) 
	{
		PPGeDrawImage(I_CROSS, 50, 220, 0, 0xFFFFFFFF);
		PPGeDrawText("OK", 100, 220, PPGE_ALIGN_LEFT, 1.0f, 0xFFFFFFFF);
	}
	PPGeDrawImage(I_CIRCLE, 300, 220, 0, 0xFFFFFFFF);
	PPGeDrawText("Cancel", 350, 220, PPGE_ALIGN_LEFT, 1.0f, 0xFFFFFFFF);
	if (!lastButtons) {
		if (buttons & (CTRL_CIRCLE)) {
			saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
			param->result = SCE_UTILITY_SAVEDATA_ERROR_LOAD_PARAM;
		}
		else if (buttons & (CTRL_CROSS)) {
			switch(param->mode)
			{
				case SCE_UTILITY_SAVEDATA_TYPE_AUTOSAVE:
				case SCE_UTILITY_SAVEDATA_TYPE_SAVE:
				case SCE_UTILITY_SAVEDATA_TYPE_LISTSAVE:
				{
					if(saveSaveData())
					{
						saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
					}
				}
				break;
				
				case SCE_UTILITY_SAVEDATA_TYPE_AUTOLOAD:
				case SCE_UTILITY_SAVEDATA_TYPE_LOAD:
				case SCE_UTILITY_SAVEDATA_TYPE_LISTLOAD:
				{
					if(loadSaveData())
					{
						saveDataDialogState = SCE_UTILITY_STATUS_FINISHED;
					}
				}
				break;
				
				default:
				break;
			}
		}
		else if (buttons & (CTRL_UP) && saveDialogInfo.menuListSelection > 0) {
			saveDialogInfo.menuListSelection--;
		}
		else if (buttons & (CTRL_DOWN) && saveDialogInfo.menuListSelection < (saveDialogInfo.saveNameListDataCount-1)) {
			saveDialogInfo.menuListSelection++;
		}
	}

	lastButtons = buttons;

	//Memory::WriteStruct(messageDialogAddr, &messageDialog);

	PPGeEnd();

	
	//__UtilityUpdate();
	return;
}


#define PSP_AV_MODULE_AVCODEC					 0
#define PSP_AV_MODULE_SASCORE					 1
#define PSP_AV_MODULE_ATRAC3PLUS				2 // Requires PSP_AV_MODULE_AVCODEC loading first
#define PSP_AV_MODULE_MPEGBASE					3 // Requires PSP_AV_MODULE_AVCODEC loading first
#define PSP_AV_MODULE_MP3							 4
#define PSP_AV_MODULE_VAUDIO						5
#define PSP_AV_MODULE_AAC							 6
#define PSP_AV_MODULE_G729							7

//TODO: Shouldn't be void
void sceUtilityLoadAvModule(u32 module)
{
	DEBUG_LOG(HLE,"sceUtilityLoadAvModule(%i)", module);
	RETURN(0);
	__KernelReSchedule("utilityloadavmodule");
}

//TODO: Shouldn't be void
void sceUtilityLoadModule(u32 module)
{
	DEBUG_LOG(HLE,"sceUtilityLoadModule(%i)", module);
	RETURN(0);
	__KernelReSchedule("utilityloadmodule");
}

typedef struct
{
	unsigned int size;	/** Size of the structure */
	int language;		/** Language */
	int buttonSwap;		/** Set to 1 for X/O button swap */
	int graphicsThread;	/** Graphics thread priority */
	int accessThread;	/** Access/fileio thread priority (SceJobThread) */
	int fontThread;		/** Font thread priority (ScePafThread) */
	int soundThread;	/** Sound thread priority */
	int result;			/** Result */
	int reserved[4];	/** Set to 0 */

} pspUtilityDialogCommon;

struct pspMessageDialog
{
	pspUtilityDialogCommon common;
	int result;
	int type;
	u32 errorNum;
	char string[512];
	u32 options;
	u32 buttonPressed;	// 0=?, 1=Yes, 2=No, 3=Back
};

void sceUtilityMsgDialogInitStart(u32 structAddr)
{
	DEBUG_LOG(HLE,"sceUtilityMsgDialogInitStart(%i)", structAddr);
	if (!Memory::IsValidAddress(structAddr))
	{
		RETURN(-1);
		return;
	}
	messageDialogAddr = structAddr;
	pspMessageDialog messageDialog;
	Memory::ReadStruct(messageDialogAddr, &messageDialog);
	if (messageDialog.type == 0) // number
	{
		INFO_LOG(HLE, "MsgDialog: %08x", messageDialog.errorNum);
	}
	else
	{
		INFO_LOG(HLE, "MsgDialog: %s", messageDialog.string);
	}
	//__UtilityInitStart();
	msgDialogState = SCE_UTILITY_STATUS_INITIALIZE;
}

void sceUtilityMsgDialogShutdownStart(u32 unknown)
{
	DEBUG_LOG(HLE,"FAKE sceUtilityMsgDialogShutdownStart(%i)", unknown);
	//__UtilityShutdownStart();
	msgDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	RETURN(0);
}

void sceUtilityMsgDialogUpdate(int animSpeed)
{
	DEBUG_LOG(HLE,"sceUtilityMsgDialogUpdate(%i)", animSpeed);

	switch (msgDialogState) {
	case SCE_UTILITY_STATUS_FINISHED:
		msgDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
		break;
	}

	if (msgDialogState != SCE_UTILITY_STATUS_RUNNING)
	{
		RETURN(0);
		return;
	}

	if (!Memory::IsValidAddress(messageDialogAddr)) {
		ERROR_LOG(HLE, "sceUtilityMsgDialogUpdate: Bad messagedialogaddr %08x", messageDialogAddr);
		RETURN(-1);
		return;
	}

	pspMessageDialog messageDialog;
	Memory::ReadStruct(messageDialogAddr, &messageDialog);
	const char *text;
	if (messageDialog.type == 0) {
		char temp[256];
		sprintf(temp, "Error code: %08x", messageDialog.errorNum);
		text = temp;
	} else {
		text = messageDialog.string;
	}

	PPGeBegin();

	PPGeDraw4Patch(I_BUTTON, 0, 0, 480, 272, 0xcFFFFFFF);
	PPGeDrawText(text, 50, 50, PPGE_ALIGN_LEFT, 0.5f, 0xFFFFFFFF);

	static u32 lastButtons = 0;
	u32 buttons = __CtrlPeekButtons();

	if (messageDialog.options & 0x10)  // yesnobutton
	{
		PPGeDrawImage(I_CROSS, 80, 220, 0, 0xFFFFFFFF);
		PPGeDrawText("Yes", 140, 220, PPGE_ALIGN_HCENTER, 1.0f, 0xFFFFFFFF);
		PPGeDrawImage(I_CIRCLE, 200, 220, 0, 0xFFFFFFFF);
		PPGeDrawText("No", 260, 220, PPGE_ALIGN_HCENTER, 1.0f, 0xFFFFFFFF);
		PPGeDrawImage(I_TRIANGLE, 320, 220, 0, 0xcFFFFFFF);
		PPGeDrawText("Back", 380, 220, PPGE_ALIGN_HCENTER, 1.0f, 0xcFFFFFFF);
		if (!lastButtons) {
			if (buttons & CTRL_TRIANGLE) {
				messageDialog.buttonPressed = 3;  // back
				msgDialogState = SCE_UTILITY_STATUS_FINISHED;
			} else if (buttons & CTRL_CROSS) {
				messageDialog.buttonPressed = 1;
				msgDialogState = SCE_UTILITY_STATUS_FINISHED;
			} else if (buttons & CTRL_CIRCLE) {
				messageDialog.buttonPressed = 2;
				msgDialogState = SCE_UTILITY_STATUS_FINISHED;
			}
		}
	}
	else
	{
		PPGeDrawImage(I_CROSS, 150, 220, 0, 0xFFFFFFFF);
		PPGeDrawText("OK", 480/2, 220, PPGE_ALIGN_HCENTER, 1.0f, 0xFFFFFFFF);
		if (!lastButtons) {
			if (buttons & (CTRL_CROSS | CTRL_CIRCLE)) {  // accept both
				messageDialog.buttonPressed = 1;
				msgDialogState = SCE_UTILITY_STATUS_FINISHED;
			}
		}
	}

	lastButtons = buttons;

	Memory::WriteStruct(messageDialogAddr, &messageDialog);

	PPGeEnd();

	RETURN(0);
}

u32 sceUtilityMsgDialogGetStatus()
{
	if (msgDialogState == SCE_UTILITY_STATUS_SHUTDOWN)
		msgDialogState = SCE_UTILITY_STATUS_NONE;
	if (msgDialogState == SCE_UTILITY_STATUS_INITIALIZE)
		msgDialogState = SCE_UTILITY_STATUS_RUNNING;
	u32 retval = msgDialogState;
	DEBUG_LOG(HLE,"sceUtilityMsgDialogGetStatus()");
	return retval;
}


// On screen keyboard

void sceUtilityOskInitStart()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityOskInitStart(%i)", PARAM(0));
	//__UtilityInitStart();
	oskDialogState = SCE_UTILITY_STATUS_INITIALIZE;
}

void sceUtilityOskShutdownStart()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityOskShutdownStart(%i)", PARAM(0));
	//__UtilityShutdownStart();
	oskDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	RETURN(0);
}

void sceUtilityOskUpdate()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityOskUpdate(%i)", PARAM(0));
	//__UtilityUpdate();
	if (oskDialogState == SCE_UTILITY_STATUS_INITIALIZE)
	{
		oskDialogState = SCE_UTILITY_STATUS_RUNNING;
	}
	else if (oskDialogState == SCE_UTILITY_STATUS_RUNNING)
	{
		oskDialogState = SCE_UTILITY_STATUS_FINISHED;
	}
	else if (oskDialogState == SCE_UTILITY_STATUS_FINISHED)
	{
		oskDialogState = SCE_UTILITY_STATUS_SHUTDOWN;
	}
	RETURN(0);
}

void sceUtilityOskGetStatus()
{
	if (oskDialogState == SCE_UTILITY_STATUS_SHUTDOWN)
		oskDialogState = SCE_UTILITY_STATUS_NONE;
	if (oskDialogState == SCE_UTILITY_STATUS_INITIALIZE)
		oskDialogState = SCE_UTILITY_STATUS_RUNNING;
	u32 retval = msgDialogState;
	DEBUG_LOG(HLE,"sceUtilityOskGetStatus()");
	RETURN(retval);
}


void sceUtilityNetconfInitStart()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityNetconfInitStart(%i)", PARAM(0));
	__UtilityInitStart();
}

void sceUtilityNetconfShutdownStart()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityNetconfShutdownStart(%i)", PARAM(0));
	__UtilityShutdownStart();
	RETURN(0);
}

void sceUtilityNetconfUpdate()
{
	DEBUG_LOG(HLE,"FAKE sceUtilityNetconfUpdate(%i)", PARAM(0));
	__UtilityUpdate();
	RETURN(0);
}

void sceUtilityNetconfGetStatus()
{
	DEBUG_LOG(HLE,"sceUtilityNetconfGetStatus()");
	RETURN(__UtilityGetStatus());
}

int sceUtilityScreenshotGetStatus()
{
	u32 retval =  0;//__UtilityGetStatus();
	DEBUG_LOG(HLE,"%i=sceUtilityScreenshotGetStatus()", retval);
	return retval;
}

int sceUtilityGamedataInstallGetStatus()
{
	u32 retval = 0;//__UtilityGetStatus();
	DEBUG_LOG(HLE,"%i=sceUtilityGamedataInstallGetStatus()", retval);
	return retval;
}

#define PSP_SYSTEMPARAM_ID_STRING_NICKNAME	1
#define PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL	2
#define PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE	3
#define PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT	4
#define PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT	5
//Timezone offset from UTC in minutes, (EST = -300 = -5 * 60)
#define PSP_SYSTEMPARAM_ID_INT_TIMEZONE		6
#define PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS	7
#define PSP_SYSTEMPARAM_ID_INT_LANGUAGE		8
/**
* #9 seems to be Region or maybe X/O button swap.
* It doesn't exist on JAP v1.0
* is 1 on NA v1.5s
* is 0 on JAP v1.5s
* is read-only
*/
#define PSP_SYSTEMPARAM_ID_INT_UNKNOWN		9

/**
* Return values for the SystemParam functions
*/
#define PSP_SYSTEMPARAM_RETVAL_OK	0
#define PSP_SYSTEMPARAM_RETVAL_FAIL	0x80110103

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL
*/
#define PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC 0
#define PSP_SYSTEMPARAM_ADHOC_CHANNEL_1		1
#define PSP_SYSTEMPARAM_ADHOC_CHANNEL_6		6
#define PSP_SYSTEMPARAM_ADHOC_CHANNEL_11	11

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE
*/
#define PSP_SYSTEMPARAM_WLAN_POWERSAVE_OFF	0
#define PSP_SYSTEMPARAM_WLAN_POWERSAVE_ON	1

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT
*/
#define PSP_SYSTEMPARAM_DATE_FORMAT_YYYYMMDD	0
#define PSP_SYSTEMPARAM_DATE_FORMAT_MMDDYYYY	1
#define PSP_SYSTEMPARAM_DATE_FORMAT_DDMMYYYY	2

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT
*/
#define PSP_SYSTEMPARAM_TIME_FORMAT_24HR	0
#define PSP_SYSTEMPARAM_TIME_FORMAT_12HR	1

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS
*/
#define PSP_SYSTEMPARAM_DAYLIGHTSAVINGS_STD	0
#define PSP_SYSTEMPARAM_DAYLIGHTSAVINGS_SAVING	1

/**
* Valid values for PSP_SYSTEMPARAM_ID_INT_LANGUAGE
*/
#define PSP_SYSTEMPARAM_LANGUAGE_JAPANESE	0
#define PSP_SYSTEMPARAM_LANGUAGE_ENGLISH	1
#define PSP_SYSTEMPARAM_LANGUAGE_FRENCH		2
#define PSP_SYSTEMPARAM_LANGUAGE_SPANISH	3
#define PSP_SYSTEMPARAM_LANGUAGE_GERMAN		4
#define PSP_SYSTEMPARAM_LANGUAGE_ITALIAN	5
#define PSP_SYSTEMPARAM_LANGUAGE_DUTCH		6
#define PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE	7
#define PSP_SYSTEMPARAM_LANGUAGE_KOREAN		8




u32 sceUtilityGetSystemParamString(u32 id, u32 destaddr, u32 unknownparam)
{
	//DEBUG_LOG(HLE,"sceUtilityGetSystemParamString(%i, %08x, %i)", id,destaddr,unknownparam);
	char *buf = (char *)Memory::GetPointer(destaddr);
	switch (id) {
	case PSP_SYSTEMPARAM_ID_STRING_NICKNAME:
		strcpy(buf, "shadow");
		break;

	default:
		return PSP_SYSTEMPARAM_RETVAL_FAIL;
	}

	return 0;
}

u32 sceUtilityGetSystemParamInt(u32 id, u32 destaddr)
{
	DEBUG_LOG(HLE,"sceUtilityGetSystemParamInt(%i, %08x)", id,destaddr);
	u32 param = 0;
	switch (id) {
	case PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL:
		param = PSP_SYSTEMPARAM_ADHOC_CHANNEL_AUTOMATIC;
		break;
	case PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE:
		param = PSP_SYSTEMPARAM_WLAN_POWERSAVE_OFF;
		break;
	case PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT:
		param = PSP_SYSTEMPARAM_DATE_FORMAT_DDMMYYYY;
		break;
	case PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT:
		param = PSP_SYSTEMPARAM_TIME_FORMAT_24HR;
		break;
	case PSP_SYSTEMPARAM_ID_INT_TIMEZONE:
		param = 60;
		break;
	case PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS:
		param = PSP_SYSTEMPARAM_TIME_FORMAT_24HR;
		break;
	case PSP_SYSTEMPARAM_ID_INT_LANGUAGE:
		param = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
		break;
	case PSP_SYSTEMPARAM_ID_INT_UNKNOWN:
		param = 1;
		break;
	default:
		return PSP_SYSTEMPARAM_RETVAL_FAIL;
	}

	Memory::Write_U32(param, destaddr);

	return 0;
}

u32 sceUtilityLoadNetModule(u32 module)
{
	DEBUG_LOG(HLE,"FAKE: sceUtilityLoadNetModule(%i)", module);
	return 0;
}

const HLEFunction sceUtility[] = 
{
	{0x1579a159, &WrapU_U<sceUtilityLoadNetModule>, "sceUtilityLoadNetModule"},
	{0xf88155f6, sceUtilityNetconfShutdownStart, "sceUtilityNetconfShutdownStart"},
	{0x4db1e739, sceUtilityNetconfInitStart, "sceUtilityNetconfInitStart"},
	{0x91e70e35, sceUtilityNetconfUpdate, "sceUtilityNetconfUpdate"},
	{0x6332aa39, sceUtilityNetconfGetStatus, "sceUtilityNetconfGetStatus"},

	{0x67af3428, &WrapV_U<sceUtilityMsgDialogShutdownStart>, "sceUtilityMsgDialogShutdownStart"},	
	{0x2ad8e239, &WrapV_U<sceUtilityMsgDialogInitStart>, "sceUtilityMsgDialogInitStart"},			
	{0x95fc253b, &WrapV_I<sceUtilityMsgDialogUpdate>, "sceUtilityMsgDialogUpdate"},				 
	{0x9a1c91d7, &WrapU_V<sceUtilityMsgDialogGetStatus>, "sceUtilityMsgDialogGetStatus"},			

	{0x9790b33c, &WrapI_V<sceUtilitySavedataShutdownStart>, "sceUtilitySavedataShutdownStart"},	 
	{0x50c4cd57, &WrapI_U<sceUtilitySavedataInitStart>, "sceUtilitySavedataInitStart"},			 
	{0xd4b95ffb, &WrapV_U<sceUtilitySavedataUpdate>, "sceUtilitySavedataUpdate"},					
	{0x8874dbe0, &WrapI_V<sceUtilitySavedataGetStatus>, "sceUtilitySavedataGetStatus"},

	{0x3dfaeba9, sceUtilityOskShutdownStart, "sceUtilityOskShutdownStart"}, 
	{0xf6269b82, sceUtilityOskInitStart, "sceUtilityOskInitStart"}, 
	{0x4b85c861, sceUtilityOskUpdate, "sceUtilityOskUpdate"}, 
	{0xf3f76017, sceUtilityOskGetStatus, "sceUtilityOskGetStatus"}, 

	{0x41e30674, 0, "sceUtilitySetSystemParamString"},
	{0x34b78343, &WrapU_UUU<sceUtilityGetSystemParamString>, "sceUtilityGetSystemParamString"}, 
	{0xA5DA2406, &WrapU_UU<sceUtilityGetSystemParamInt>, "sceUtilityGetSystemParamInt"},
	{0xc492f751, 0, "sceUtilityGameSharingInitStart"}, 
	{0xefc6f80f, 0, "sceUtilityGameSharingShutdownStart"}, 
	{0x7853182d, 0, "sceUtilityGameSharingUpdate"}, 
	{0x946963f3, 0, "sceUtilityGameSharingGetStatus"}, 
	{0x2995d020, 0, "sceUtility_2995d020"}, 
	{0xb62a4061, 0, "sceUtility_b62a4061"}, 
	{0xed0fad38, 0, "sceUtility_ed0fad38"}, 
	{0x88bc7406, 0, "sceUtility_88bc7406"}, 
	{0x45c18506, 0, "sceUtilitySetSystemParamInt"}, 
	{0x5eee6548, 0, "sceUtilityCheckNetParam"}, 
	{0x434d4b3a, 0, "sceUtilityGetNetParam"}, 
	{0x64d50c56, 0, "sceUtilityUnloadNetModule"}, 
	{0x4928bd96, 0, "sceUtilityMsgDialogAbort"}, 
	{0xbda7d894, 0, "sceUtilityHtmlViewerGetStatus"}, 
	{0xcdc3aa41, 0, "sceUtilityHtmlViewerInitStart"}, 
	{0xf5ce1134, 0, "sceUtilityHtmlViewerShutdownStart"}, 
	{0x05afb9e4, 0, "sceUtilityHtmlViewerUpdate"}, 
	{0xc629af26, &WrapV_U<sceUtilityLoadAvModule>, "sceUtilityLoadAvModule"}, 
	{0xf7d8d092, 0, "sceUtilityUnloadAvModule"},
	{0x2a2b3de0, &WrapV_U<sceUtilityLoadModule>, "sceUtilityLoadModule"},
	{0xe49bfe92, 0, "sceUtilityUnloadModule"},
	{0x0251B134, 0, "sceUtilityScreenshotInitStart"},
	{0xF9E0008C, 0, "sceUtilityScreenshotShutdownStart"},
	{0xAB083EA9, 0, "sceUtilityScreenshotUpdate"},
	{0xD81957B7, &WrapI_V<sceUtilityScreenshotGetStatus>, "sceUtilityScreenshotGetStatus"},
	{0x86A03A27, 0, "sceUtilityScreenshotContStart"},

	{0x0D5BC6D2, 0, "sceUtilityLoadUsbModule"},
	{0xF64910F0, 0, "sceUtilityUnloadUsbModule"},

	{0x24AC31EB, 0, "sceUtilityGamedataInstallInitStart"},
	{0x32E32DCB, 0, "sceUtilityGamedataInstallShutdownStart"},
	{0x4AECD179, 0, "sceUtilityGamedataInstallUpdate"},
	{0xB57E95D9, &WrapI_V<sceUtilityGamedataInstallGetStatus>, "sceUtilityGamedataInstallGetStatus"},
	{0x180F7B62, 0, "sceUtilityGamedataInstallAbortFunction"},

	{0x16D02AF0, 0, "sceUtilityNpSigninInitStart"},
	{0xE19C97D6, 0, "sceUtilityNpSigninShutdownStart"},
	{0xF3FBC572, 0, "sceUtilityNpSigninUpdate"},
	{0x86ABDB1B, 0, "sceUtilityNpSigninGetStatus"},
};

void Register_sceUtility()
{
	RegisterModule("sceUtility", ARRAY_SIZE(sceUtility), sceUtility);
}
