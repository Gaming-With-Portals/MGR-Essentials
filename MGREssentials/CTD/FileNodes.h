#pragma once
#define NOMINMAX
#include <string>
#include <vector>
#include <Windows.h>

#include "BinaryHandler.h"
#include "tinyxml2.h"
#include <math.h>
#include <algorithm>
#include <unordered_map>
enum FileNodeTypes {
	DEFAULT,
	UNKNOWN,
	MOT,
	BXM,
	WEM,
	WMB,
	BNK,
	DAT,
	WTB,
	LY2,
	UID,
	UVD,
	TRG,
	EST,
	B1EFF,
	B1PHYS,
	CT2,
	ACB,
	SDX
};

namespace HelperFunction {
	int Align(int value, int alignment);
}

class HashDataContainer {
public:
	std::vector<int> Hashes;
	std::vector<short> Indices;
	std::vector<short> Offsets;
	int Shift;
	int StructSize;
};

namespace BXMInternal {
	extern const std::vector<std::string> possibleParams;

	std::vector<std::string> SplitString(const std::string& str, char delimiter);

	struct XMLAttribute {
		std::string value = "";
		std::string name = "";
	};

	struct XMLNode {
		std::string name = "";
		std::string value = "";
		XMLNode* parent;
		std::vector<XMLNode*> childNodes;
		std::vector<XMLAttribute*> childAttributes;

	};

}

class FileNode {
public:
	std::string fileName;
	std::string fileExtension;
	std::vector<FileNode*> children;
	std::vector<char> fileData;
	FileNodeTypes nodeType;
	FileNode* parent = nullptr;
	LPCWSTR fileFilter = L"All Files(*.*)\0*.*;\0";
	bool loadFailed = false;
	bool isEdited = false;
	bool canHaveChildren = false;
	static FileNode* selectedNode;


	std::string fileIcon = "";
	bool fileIsBigEndian = false;
	//ImVec4 TextColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	FileNode(std::string fName);

	virtual ~FileNode();


	virtual void LoadFile() = 0;
	virtual void SaveFile() = 0;

	void SetFileData(const std::vector<char>& data);

	const std::vector<char>& GetFileData() const;
};

class UnkFileNode : public FileNode {
public:
	UnkFileNode(std::string fName);
	void LoadFile() override;
	void SaveFile() override;
};

class DatFileNode : public FileNode {
public:
	std::unordered_map<unsigned int, unsigned int> textureInfo;

	DatFileNode(std::string fName);

	void LoadFile() override;



	void SaveFile() override;
};
