#include "FileNodes.h"
#include <queue>
#include "CRC32.h"


int IntLength(int value) {
	int length = 0;
	while (value > 0) {
		value >>= 1;
		length++;
	}
	return length;
}

int HelperFunction::Align(int value, int alignment) {
	return (value + (alignment - 1)) & ~(alignment - 1);
}

FileNode::FileNode(std::string fName) {
	fileName = fName;
	fileExtension = fileName.substr(fileName.find_last_of(".") + 1);
}

FileNode::~FileNode() {

}


void FileNode::SetFileData(const std::vector<char>& data) {
	fileData = data;
}

const std::vector<char>& FileNode::GetFileData() const {
	return fileData;
}



DatFileNode::DatFileNode(std::string fName) : FileNode(fName) {
	fileIcon = "";
	nodeType = DAT;
	fileFilter = L"Platinum File Container(*.dat, *.dtt, *.eff, *.evn, *.eft)\0*.dat;*.dtt;*.eff;*.evn;*.eft;\0";
	canHaveChildren = true;
}

void DatFileNode::LoadFile() {
	BinaryReader reader(fileData, false);
	reader.Seek(0x4);
	unsigned int FileCount = reader.ReadUINT32();
	unsigned int PositionsOffset = reader.ReadUINT32();
	reader.Skip(sizeof(unsigned int)); // TODO: ExtensionsOffset
	unsigned int NamesOffset = reader.ReadUINT32();
	unsigned int SizesOffset = reader.ReadUINT32();
	reader.Skip(sizeof(unsigned int)); // TODO: HashMapOffset

	reader.Seek(PositionsOffset);
	std::vector<int> offsets;
	for (unsigned int f = 0; f < FileCount; f++) {
		offsets.push_back(reader.ReadUINT32());
	}

	reader.Seek(NamesOffset);
	int nameLength = reader.ReadUINT32();
	std::vector<std::string> names;
	for (unsigned int f = 0; f < FileCount; f++) {
		std::string temp_name = reader.ReadString(nameLength);
		temp_name.erase(std::remove(temp_name.begin(), temp_name.end(), '\0'), temp_name.end());
		names.push_back(temp_name);
	}

	reader.Seek(SizesOffset);
	std::vector<int> sizes;
	for (unsigned int f = 0; f < FileCount; f++) {
		sizes.push_back(reader.ReadUINT32());
	}

	for (unsigned int f = 0; f < FileCount; f++) {
		reader.Seek(offsets[f]);
		

		//FileNode* childNode = HelperFunction::LoadNode(names[f], reader.ReadBytes(sizes[f]), fileIsBigEndian, fileIsBigEndian);
		FileNode* childNode = new UnkFileNode(names[f]);
		childNode->SetFileData(reader.ReadBytes(sizes[f]));

		childNode->parent = this;
		if (childNode) {
			children.push_back(childNode);
		}

	}


}

UnkFileNode::UnkFileNode(std::string fName) : FileNode(fName) {
	fileName = fName;
}

void UnkFileNode::LoadFile() {
}

void UnkFileNode::SaveFile() {
}

void DatFileNode::SaveFile() {



	int longestName = 0;

	for (FileNode* child : children) {
		child->SaveFile();
		if (child->fileName.length() > longestName) {
			longestName = static_cast<int>(child->fileName.length() + 1);
		}
	}

	CRC32 crc32;

	std::vector<std::string> fileNames;
	for (FileNode* node : children) {
		fileNames.push_back(node->fileName);
	}

	int shift = std::min(31, 32 - IntLength(static_cast<int>(fileNames.size())));
	int bucketSize = 1 << (31 - shift);

	std::vector<short> bucketTable(bucketSize, -1);

	std::vector<std::pair<int, short>> hashTuple;
	for (int i = 0; i < fileNames.size(); ++i) {
		//int hashValue = crc32.HashToUInt32(fileNames[i]) & 0x7FFFFFFF;
		int hashValue = ComputeHash(fileNames[i], crc32);
		hashTuple.push_back({ hashValue, static_cast<short>(i) });
	}

	// Sort the hash tuples based on shifted hash values
	std::sort(hashTuple.begin(), hashTuple.end(), [shift](const std::pair<int, short>& a, const std::pair<int, short>& b) {
		return (a.first >> shift) < (b.first >> shift);
		});

	// Populate bucket table with the first unique index for each bucket
	for (int i = 0; i < fileNames.size(); ++i) {
		int bucketIndex = hashTuple[i].first >> shift;
		if (bucketTable[bucketIndex] == -1) {
			bucketTable[bucketIndex] = static_cast<short>(i);
		}
	}

	// Create the result object with the hash data
	HashDataContainer hashData;
	hashData.Shift = shift;
	hashData.Offsets = bucketTable;
	hashData.Hashes.reserve(hashTuple.size());
	hashData.Indices.reserve(hashTuple.size());

	for (const auto& tuple : hashTuple) {
		hashData.Hashes.push_back(tuple.first);
		hashData.Indices.push_back(tuple.second);
	}

	hashData.StructSize = static_cast<int>(4 + 2 * bucketTable.size() + 4 * hashTuple.size() + 2 * hashTuple.size());

	BinaryWriter* writer = new BinaryWriter();
	writer->SetEndianess(fileIsBigEndian);
	writer->WriteString("DAT");
	writer->WriteByteZero();
	int fileCount = static_cast<int>(children.size());

	int positionsOffset = 0x20;
	int extensionsOffset = positionsOffset + 4 * fileCount;
	int namesOffset = extensionsOffset + 4 * fileCount;
	int sizesOffset = namesOffset + (fileCount * longestName) + 6;
	int hashMapOffset = sizesOffset + 4 * fileCount;

	writer->WriteUINT32(fileCount);
	writer->WriteUINT32(positionsOffset);
	writer->WriteUINT32(extensionsOffset);
	writer->WriteUINT32(namesOffset);
	writer->WriteUINT32(sizesOffset);
	writer->WriteUINT32(hashMapOffset);
	writer->WriteUINT32(0);

	writer->Seek(positionsOffset);
	for (FileNode* child : children) {
		(void)child; // TODO: If child isn't gonna be used in a future patch, remove it entirely instead of discarding.
		writer->WriteUINT32(0);
	}


	writer->Seek(extensionsOffset);
	for (FileNode* child : children) {
		writer->WriteString(child->fileExtension);
		writer->WriteByteZero();
	}

	writer->Seek(namesOffset);
	writer->WriteUINT32(longestName);
	for (FileNode* child : children) {
		writer->WriteString(child->fileName);
		for (int i = 0; i < longestName - child->fileName.length(); ++i) {
			writer->WriteByteZero();
		}
	}
	// Pad
	writer->WriteINT16(0);

	writer->Seek(sizesOffset);
	for (FileNode* child : children) {
		writer->WriteUINT32(static_cast<uint32_t>(child->fileData.size()));
	}

	writer->Seek(hashMapOffset);

	// Prepare for hash writing
	writer->WriteUINT32(hashData.Shift);
	writer->WriteUINT32(16);
	writer->WriteUINT32(16 + static_cast<uint32_t>(hashData.Offsets.size()) * 2);
	writer->WriteUINT32(16 + static_cast<uint32_t>(hashData.Offsets.size()) * 2 + static_cast<uint32_t>(hashData.Hashes.size()) * 4);

	for (int i = 0; i < hashData.Offsets.size(); i++)
		writer->WriteINT16(hashData.Offsets[i]);

	for (int i = 0; i < fileCount; i++)
		writer->WriteINT32(hashData.Hashes[i]);

	for (int i = 0; i < fileCount; i++)
		writer->WriteINT16(hashData.Indices[i]);

	std::vector<int> offsets;
	for (FileNode* child : children) {
		(void)child; // TODO: If child isn't gonna be used in a future patch, remove it entirely instead of discarding.

		int targetPosition = HelperFunction::Align(static_cast<int>(writer->Tell()), 1024);
		int padding = targetPosition - static_cast<int>(writer->GetData().size());
		if (padding > 0) { // TODO: Replace this with an skip function
			std::vector<char> zeroPadding(padding, 0);
			writer->WriteBytes(zeroPadding);
		}


		offsets.push_back(static_cast<int>(writer->Tell()));
		writer->WriteBytes(child->fileData);

	}

	writer->Seek(positionsOffset);
	for (int i = 0; i < fileCount; i++) {
		writer->WriteUINT32(offsets[i]);
	}

	fileData = writer->GetData();
}