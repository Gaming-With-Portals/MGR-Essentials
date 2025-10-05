#include "pch.h"
#include <assert.h>
#include "gui.h"
#include <Events.h>
#include "include/MinHook.h"
#include "FileRead.h"
#include "CTD/FileNodes.h"
#include <fstream>


#include "imgui/imgui.h"

#define CREATE_HOOK(ret, callconv, name, ...) typedef ret(callconv *name##_t)(__VA_ARGS__); static name##_t o##name = NULL; ret callconv hk##name(__VA_ARGS__)

#define CREATE_THISCALL(ret, name, thus, ...) typedef ret(__thiscall *name##_t)(thus, __VA_ARGS__); static name##_t o##name = NULL; ret __fastcall hk##name(thus self, void *, __VA_ARGS__)

#define HOOK(address, name) MH_CreateHook(LPVOID(address), hk##name, (LPVOID*)&o##name); MH_EnableHook(LPVOID(address));



struct FmergeNameTableHeader
{
	uint32_t stride;

	char* getName(size_t index)
	{
		return reinterpret_cast<char*>(this + 1) + index * stride;
	}
};

struct FmergeHeader
{
	char identifier[4];
	uint32_t fileNum;
	uint32_t offsetTableOffset;
	uint32_t extensionTableOffset;
	uint32_t fileNameTableOffset;
	uint32_t sizeTableOffset;
	uint32_t padding0;
	uint32_t padding1;

	uint32_t* getOffset(size_t index)
	{
		return reinterpret_cast<uint32_t*>(identifier + offsetTableOffset) + index;
	}

	char* getExtension(size_t index)
	{
		return reinterpret_cast<char*>(identifier + extensionTableOffset) + sizeof(char[4]) * index;
	}

	FmergeNameTableHeader* getFileNameTable()
	{
		return reinterpret_cast<FmergeNameTableHeader*>(identifier + fileNameTableOffset);
	}

	uint32_t* getSize(size_t index)
	{
		return reinterpret_cast<uint32_t*>(identifier + sizeTableOffset) + index;
	}
};


static size_t alignUp(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

void RecursiveRepack(const char* dirPath, DatFileNode* node, bool& did_edit) { // fire ass name holy hell
	char filter[MAX_PATH];
	sprintf_s(filter, "%s\\*", dirPath);

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(filter, &findData);
	if (hFind == INVALID_HANDLE_VALUE) return;

	do {
		if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
			continue;

		char fullPath[MAX_PATH];
		sprintf_s(fullPath, "%s\\%s", dirPath, findData.cFileName);

		FileNode* counterpart = nullptr;
		for (FileNode* child : node->children) {
			if (strcmp(findData.cFileName, child->fileName.c_str()) == 0) {
				counterpart = child;
				break;
			}
		}

		if (counterpart == nullptr) continue;

		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// [FILE]
			std::ifstream file(fullPath, std::ios::binary);
			if (file) {
				std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
					std::istreambuf_iterator<char>());
				if (buffer.empty()) {
					did_edit = true;
					auto it = std::find(node->children.begin(), node->children.end(), counterpart);
					if (it != node->children.end()) node->children.erase(it);
					delete counterpart;
				}
				else {
					counterpart->SetFileData(buffer);
					did_edit = true;
				}
			}
		}
		else {
			// [DIRECTORY]
			if (strcmp((char*)counterpart->GetFileData().data(), "DAT") == 0) {
				DatFileNode* subNode = new DatFileNode("");
				subNode->SetFileData(counterpart->GetFileData());
				subNode->LoadFile();

				RecursiveRepack(fullPath, subNode, did_edit);

				if (did_edit) {
					subNode->SaveFile();
					std::vector<char> newData = subNode->GetFileData();
					counterpart->SetFileData(newData);
				}

				delete subNode;
			}
			else {
				RecursiveRepack(fullPath, node, did_edit);
			}
		}

	} while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
}



CREATE_THISCALL(void, RegisterFile, FileRead::Work*) {

	if (strcmp((char*)self->m_Filedata, "DAT") == 0) {

		char dirPath[MAX_PATH];
		strcpy_s(dirPath, self->m_Filename);

		char* ext = strstr(dirPath, ".dat");
		if (ext == nullptr) {
			ext = strstr(dirPath, ".dtt");
		}
		if (ext != nullptr) {
			*ext = '\0';
		}

		char filter[MAX_PATH];
		sprintf_s(filter, "%s\\*", dirPath);


		

		WIN32_FIND_DATAA findData;
		HANDLE hFind = FindFirstFileA(filter, &findData);
		bool did_edit = false;
		if (hFind != INVALID_HANDLE_VALUE) {
			DatFileNode* node = new DatFileNode("");
			std::vector<char> buffer((char*)self->m_Filedata, (char*)self->m_Filedata + self->m_nFilesize);
			node->SetFileData(buffer);
			node->LoadFile();

			bool did_edit = false;
			RecursiveRepack(dirPath, node, did_edit);

			if (did_edit) {
				node->SaveFile();
				void* file_allocator = self->m_Allocator->AllocateMemory(node->GetFileData().size(), 0x1000, 0x10, 0);
				if (file_allocator) {
					memcpy(file_allocator, node->GetFileData().data(), node->GetFileData().size());
				}
				self->m_Allocator->free(self->m_Filedata, self->m_nFilesize);
				self->m_Filedata = file_allocator;
				self->m_nFilesize = node->GetFileData().size();
			}

			delete node;
		}
	}




	oRegisterFile(self);
}

class Plugin
{
public:
	static inline void InitGUI()
	{
		Events::OnDeviceReset.before += gui::OnReset::Before;
		Events::OnDeviceReset.after += gui::OnReset::After;
		Events::OnEndScene += gui::OnEndScene;
		/* // Or if you want to switch it to Present
		Events::OnPresent += gui::OnEndScene;
		*/
	}

	Plugin()
	{
		InitGUI();
		MH_Initialize();
		HOOK(shared::base + 0xA9CBC0, RegisterFile);
		// and here's your code
	}
} plugin;

void gui::RenderWindow()
{

}