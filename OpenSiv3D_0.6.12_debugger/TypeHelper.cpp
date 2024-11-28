#include <Windows.h>
#include <DbgHelp.h>
#include "TypeHelper.hpp"
#include "ProcessHandle.hpp"

CBaseTypeEnum GetCBaseType(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetBaseTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetPointerTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetArrayTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetEnumTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetNameableTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetUDTTypeName(const ProcessHandle& process, int typeID, size_t modBase);
std::wstring GetFunctionTypeName(const ProcessHandle& process, int typeID, size_t modBase);

char ConvertToSafeChar(char ch);
wchar_t ConvertToSafeWChar(wchar_t ch);
std::wstring GetBaseTypeValue(const ProcessHandle& process, int typeID, size_t modBase, const BYTE* pData);
std::wstring GetPointerTypeValue(int typeID, size_t modBase, const BYTE* pData);
std::wstring GetEnumTypeValue(const ProcessHandle& process, int typeID, size_t modBase, const BYTE* pData);
std::wstring GetArrayTypeValue(const ProcessHandle& process, int typeID, size_t modBase, size_t address, const BYTE* pData);
std::wstring GetUDTTypeValue(const ProcessHandle& process, int typeID, size_t modBase, size_t address, const BYTE* pData);
BOOL GetDataMemberInfo(const ProcessHandle& process, DWORD memberID, size_t modBase, size_t address, const BYTE* pData, std::wostringstream& valueBuilder);
BOOL VariantEqual(VARIANT var, CBaseTypeEnum cBaseType, const BYTE* data);


struct BaseTypeEntry {

	CBaseTypeEnum type;
	const LPCWSTR name;

} g_baseTypeNameMap[] = {

	{ cbtNone, TEXT("<no-type>") },
	{ cbtVoid, TEXT("void") },
	{ cbtBool, TEXT("bool") },
	{ cbtChar, TEXT("char") },
	{ cbtUChar, TEXT("unsigned char") },
	{ cbtWChar, TEXT("wchar_t") },
	{ cbtShort, TEXT("short") },
	{ cbtUShort, TEXT("unsigned short") },
	{ cbtInt, TEXT("int") },
	{ cbtUInt, TEXT("unsigned int") },
	{ cbtLong, TEXT("long") },
	{ cbtULong, TEXT("unsigned long") },
	{ cbtLongLong, TEXT("long long") },
	{ cbtULongLong, TEXT("unsigned long long") },
	{ cbtFloat, TEXT("float") },
	{ cbtDouble, TEXT("double") },
	{ cbtEnd, TEXT("") },
};

bool IsSimpleType(const ProcessHandle& process, DWORD typeID, size_t modBase)
{
	DWORD symTag;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_SYMTAG,
		&symTag);

	switch (symTag)
	{
	case SymTagBaseType:
	case SymTagPointerType:
	case SymTagEnum:
		return true;

	default:
		return false;
	}
}

std::wstring GetTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	DWORD typeTag;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_SYMTAG,
		&typeTag);

	switch (typeTag) {

	case SymTagBaseType:
		return GetBaseTypeName(process, typeID, modBase);

	case SymTagPointerType:
		return GetPointerTypeName(process, typeID, modBase);

	case SymTagArrayType:
		return GetArrayTypeName(process, typeID, modBase);

	case SymTagUDT:
		return GetUDTTypeName(process, typeID, modBase);

	case SymTagEnum:
		return GetEnumTypeName(process, typeID, modBase);

	case SymTagFunctionType:
		return GetFunctionTypeName(process, typeID, modBase);

	default:
		return L"??";
	}
}

std::wstring GetBaseTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	CBaseTypeEnum baseType = GetCBaseType(process, typeID, modBase);

	int index = 0;

	while (g_baseTypeNameMap[index].type != cbtEnd)
	{
		if (g_baseTypeNameMap[index].type == baseType)
		{
			break;
		}

		++index;
	}

	return g_baseTypeNameMap[index].name;
}

// C/C++基本型の列挙を取得する
// typeIDがすでに基本型のIDであると仮定する
CBaseTypeEnum GetCBaseType(const ProcessHandle& process, int typeID, size_t modBase)
{

	// BaseTypeEnumを取得する
	DWORD baseType;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_BASETYPE,
		&baseType);

	// 基本型の長さを取得する
	ULONG64 length;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_LENGTH,
		&length);

	switch (baseType)
	{

	case btVoid:
		return cbtVoid;

	case btChar:
		return cbtChar;

	case btWChar:
		return cbtWChar;

	case btInt:
		switch (length)
		{
		case 2:  return cbtShort;
		case 4:  return cbtInt;
		default: return cbtLongLong;
		}

	case btUInt:
		switch (length)
		{
		case 1:  return cbtUChar;
		case 2:  return cbtUShort;
		case 4:  return cbtUInt;
		default: return cbtULongLong;
		}

	case btFloat:
		switch (length)
		{
		case 4:  return cbtFloat;
		default: return cbtDouble;
		}

	case btBool:
		return cbtBool;

	case btLong:
		return cbtLong;

	case btULong:
		return cbtULong;

	default:
		return cbtNone;
	}
}

std::wstring GetPointerTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	// ポインタ型か参照型かを取得する
	BOOL isReference;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_IS_REFERENCE,
		&isReference);

	// ポインタが指すオブジェクトの型のtypeIDを取得する
	DWORD innerTypeID;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_TYPEID,
		&innerTypeID);

	return GetTypeName(process, innerTypeID, modBase) + (isReference == TRUE ? TEXT("&") : TEXT("*"));
}

std::wstring GetArrayTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	// 配列要素のTypeIDを取得する
	DWORD innerTypeID;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_TYPEID,
		&innerTypeID);

	// 配列要素の個数を取得する
	DWORD elemCount;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_COUNT,
		&elemCount);

	std::wostringstream strBuilder;

	strBuilder << GetTypeName(process, innerTypeID, modBase) << TEXT('[') << elemCount << TEXT(']');

	return strBuilder.str();
}

std::wstring GetFunctionTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	std::wostringstream nameBuilder;

	// パラメータの数を取得する
	DWORD paramCount;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_CHILDRENCOUNT,
		&paramCount);

	// 戻り値の名前を取得する
	DWORD returnTypeID;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_TYPEID,
		&returnTypeID);

	nameBuilder << GetTypeName(process, returnTypeID, modBase);

	// 各パラメータの名前を取得する
	BYTE* pBuffer = (BYTE*)malloc(sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * paramCount);
	TI_FINDCHILDREN_PARAMS* pFindParams = (TI_FINDCHILDREN_PARAMS*)pBuffer;
	pFindParams->Count = paramCount;
	pFindParams->Start = 0;

	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_FINDCHILDREN,
		pFindParams);

	nameBuilder << TEXT('(');

	for (int index = 0; index != paramCount; ++index)
	{
		DWORD paramTypeID;
		SymGetTypeInfo(
			process.getHandle(),
			modBase,
			pFindParams->ChildId[index],
			TI_GET_TYPEID,
			&paramTypeID);

		if (index != 0) {
			nameBuilder << TEXT(", ");
		}

		nameBuilder << GetTypeName(process, paramTypeID, modBase);
	}

	nameBuilder << TEXT(')');

	free(pBuffer);

	return nameBuilder.str();
}

std::wstring GetEnumTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	return GetNameableTypeName(process, typeID, modBase);
}

std::wstring GetUDTTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	return GetNameableTypeName(process, typeID, modBase);
}

std::wstring GetNameableTypeName(const ProcessHandle& process, int typeID, size_t modBase)
{
	WCHAR* pBuffer;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_SYMNAME,
		&pBuffer);

	std::wstring typeName(pBuffer);

	LocalFree(pBuffer);

	return typeName;
}

// 指定されたアドレスのメモリを取得し、対応する型の形式で表示する
std::wstring GetTypeValue(const ProcessHandle& process, int typeID, size_t modBase, size_t address, const BYTE* pData)
{
	DWORD typeTag;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_SYMTAG,
		&typeTag);

	switch (typeTag)
	{

	case SymTagBaseType:
		return GetBaseTypeValue(process, typeID, modBase, pData);

	case SymTagPointerType:
		return GetPointerTypeValue(typeID, modBase, pData);

	case SymTagEnum:
		return GetEnumTypeValue(process, typeID, modBase, pData);

	case SymTagArrayType:
		return GetArrayTypeValue(process, typeID, modBase, address, pData);

	case SymTagUDT:
		return GetUDTTypeValue(process, typeID, modBase, address, pData);

	case SymTagTypedef:

		// 実際の型のIDを取得する
		DWORD actTypeID;
		SymGetTypeInfo(
			process.getHandle(),
			modBase,
			typeID,
			TI_GET_TYPEID,
			&actTypeID);

		return GetTypeValue(process, actTypeID, modBase, address, pData);

	default:
		return L"??";
	}
}

std::wstring GetBaseTypeValue(const ProcessHandle& process, int typeID, size_t modBase, const BYTE* pData)
{
	CBaseTypeEnum cBaseType = GetCBaseType(process, typeID, modBase);

	return GetCBaseTypeValue(cBaseType, pData);
}

std::wstring GetCBaseTypeValue(CBaseTypeEnum cBaseType, const BYTE* pData)
{
	std::wostringstream valueBuilder;

	switch (cBaseType)
	{

	case cbtNone:
		valueBuilder << TEXT("??");
		break;

	case cbtVoid:
		valueBuilder << TEXT("??");
		break;

	case cbtBool:
		valueBuilder << (*pData == 0 ? L"false" : L"true");
		break;

	case cbtChar:
		valueBuilder << ConvertToSafeChar(*((char*)pData));
		break;

	case cbtUChar:
		valueBuilder << std::hex
			<< std::uppercase
			<< std::setw(2)
			<< std::setfill(TEXT('0'))
			<< *((unsigned char*)pData);
		break;

	case cbtWChar:
		valueBuilder << ConvertToSafeWChar(*((wchar_t*)pData));
		break;

	case cbtShort:
		valueBuilder << *((short*)pData);
		break;

	case cbtUShort:
		valueBuilder << *((unsigned short*)pData);
		break;

	case cbtInt:
		valueBuilder << *((int*)pData);
		break;

	case cbtUInt:
		valueBuilder << *((unsigned int*)pData);
		break;

	case cbtLong:
		valueBuilder << *((long*)pData);
		break;

	case cbtULong:
		valueBuilder << *((unsigned long*)pData);
		break;

	case cbtLongLong:
		valueBuilder << *((long long*)pData);
		break;

	case cbtULongLong:
		valueBuilder << *((unsigned long long*)pData);
		break;

	case cbtFloat:
		valueBuilder << *((float*)pData);
		break;

	case cbtDouble:
		valueBuilder << *((double*)pData);
		break;
	}

	return valueBuilder.str();
}

std::wstring GetPointerTypeValue(int typeID, size_t modBase, const BYTE* pData)
{
	std::wostringstream valueBuilder;

	valueBuilder << std::hex << std::uppercase << std::setfill(TEXT('0')) << std::setw(8) << *((DWORD*)pData);

	return valueBuilder.str();
}

std::wstring GetEnumTypeValue(const ProcessHandle& process, int typeID, size_t modBase, const BYTE* pData)
{
	std::wstring valueName;

	// 列挙値の基本型を取得する
	CBaseTypeEnum cBaseType = GetCBaseType(process, typeID, modBase);

	// 列挙値の個数を取得する
	DWORD childrenCount;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_CHILDRENCOUNT,
		&childrenCount);

	// 各列挙値を取得する
	TI_FINDCHILDREN_PARAMS* pFindParams =
		(TI_FINDCHILDREN_PARAMS*)malloc(sizeof(TI_FINDCHILDREN_PARAMS) + childrenCount * sizeof(ULONG));
	pFindParams->Start = 0;
	pFindParams->Count = childrenCount;

	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_FINDCHILDREN,
		pFindParams);

	for (int index = 0; index != childrenCount; ++index) {

		// 列挙値を取得する
		VARIANT enumValue;
		SymGetTypeInfo(
			process.getHandle(),
			modBase,
			pFindParams->ChildId[index],
			TI_GET_VALUE,
			&enumValue);

		if (VariantEqual(enumValue, cBaseType, pData) == TRUE) {

			// 列挙値の名前を取得する
			WCHAR* pBuffer;
			SymGetTypeInfo(
				process.getHandle(),
				modBase,
				pFindParams->ChildId[index],
				TI_GET_SYMNAME,
				&pBuffer);

			valueName = pBuffer;
			LocalFree(pBuffer);

			break;
		}
	}

	free(pFindParams);

	// 対応する列挙値が見つからなかった場合、基本型の値を表示する
	if (valueName.length() == 0)
	{
		valueName = GetBaseTypeValue(process, typeID, modBase, pData);
	}

	return valueName;
}

// VARIANT型の変数とメモリ領域のデータを比較し、等しいかどうかを判断する。
// メモリ領域のデータ型はcBaseTypeパラメータによって指定される
BOOL VariantEqual(VARIANT var, CBaseTypeEnum cBaseType, const BYTE* pData)
{
	switch (cBaseType)
	{
	case cbtChar:
		return var.cVal == *((char*)pData);

	case cbtUChar:
		return var.bVal == *((unsigned char*)pData);

	case cbtShort:
		return var.iVal == *((short*)pData);

	case cbtWChar:
	case cbtUShort:
		return var.uiVal == *((unsigned short*)pData);

	case cbtUInt:
		return var.uintVal == *((int*)pData);

	case cbtLong:
		return var.lVal == *((long*)pData);

	case cbtULong:
		return var.ulVal == *((unsigned long*)pData);

	case cbtLongLong:
		return var.llVal == *((long long*)pData);

	case cbtULongLong:
		return var.ullVal == *((unsigned long long*)pData);

	case cbtInt:
	default:
		return var.intVal == *((int*)pData);
	}
}

// 配列型変数の値を取得する
// 最大32個の要素の値のみを取得する
std::wstring GetArrayTypeValue(const ProcessHandle& process, int typeID, size_t modBase, size_t address, const BYTE* pData)
{
	// 要素の個数を取得し、32個を超える場合は32に設定する
	DWORD elemCount;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_COUNT,
		&elemCount);

	elemCount = elemCount > 32 ? 32 : elemCount;

	// 配列要素のTypeIDを取得する
	DWORD innerTypeID;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_TYPEID,
		&innerTypeID);

	// 配列要素の長さを取得する
	ULONG64 elemLen;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		innerTypeID,
		TI_GET_LENGTH,
		&elemLen);

	std::wostringstream valueBuilder;

	for (int index = 0; index != elemCount; ++index)
	{
		DWORD elemOffset = DWORD(index * elemLen);

		valueBuilder << TEXT("  [") << index << TEXT("]  ")
			<< GetTypeValue(process, innerTypeID, modBase, address + elemOffset, pData + index * elemLen);

		if (index != elemCount - 1) {
			valueBuilder << std::endl;
		}
	}

	return valueBuilder.str();
}

// ユーザー定義型の値を取得する
std::wstring GetUDTTypeValue(const ProcessHandle& process, int typeID, size_t modBase, size_t address, const BYTE* pData)
{
	std::wostringstream valueBuilder;

	// メンバーの個数を取得する
	DWORD memberCount;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_GET_CHILDRENCOUNT,
		&memberCount);

	// メンバー情報を取得する
	TI_FINDCHILDREN_PARAMS* pFindParams =
		(TI_FINDCHILDREN_PARAMS*)malloc(sizeof(TI_FINDCHILDREN_PARAMS) + memberCount * sizeof(ULONG));
	pFindParams->Start = 0;
	pFindParams->Count = memberCount;

	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		typeID,
		TI_FINDCHILDREN,
		pFindParams);

	// メンバーを走査する
	for (int index = 0; index != memberCount; ++index)
	{
		BOOL isDataMember = GetDataMemberInfo(
			process,
			pFindParams->ChildId[index],
			modBase,
			address,
			pData,
			valueBuilder);

		if (isDataMember == TRUE) {
			valueBuilder << std::endl;
		}
	}

	valueBuilder.seekp(-1, valueBuilder.end);
	valueBuilder.put(0);

	return valueBuilder.str();
}

// データメンバーの情報を取得する
// メンバーがデータメンバーである場合はTRUEを返す
// それ以外の場合はFALSEを返す
BOOL GetDataMemberInfo(const ProcessHandle& process, DWORD memberID, size_t modBase, size_t address, const BYTE* pData, std::wostringstream& valueBuilder)
{
	DWORD memberTag;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		memberID,
		TI_GET_SYMTAG,
		&memberTag);

	if (memberTag != SymTagData && memberTag != SymTagBaseClass)
	{
		return FALSE;
	}

	valueBuilder << TEXT("  ");

	DWORD memberTypeID;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		memberID,
		TI_GET_TYPEID,
		&memberTypeID);

	// 型を出力する
	valueBuilder << GetTypeName(process, memberTypeID, modBase);

	// 名前を出力する
	if (memberTag == SymTagData)
	{
		WCHAR* name;
		SymGetTypeInfo(
			process.getHandle(),
			modBase,
			memberID,
			TI_GET_SYMNAME,
			&name);

		valueBuilder << TEXT("  ") << name;

		LocalFree(name);
	}
	else
	{
		valueBuilder << TEXT("  <base-class>");
	}

	// 長さを出力する
	ULONG64 length;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		memberTypeID,
		TI_GET_LENGTH,
		&length);

	valueBuilder << TEXT("  ") << length;

	// アドレスを出力する
	DWORD offset;
	SymGetTypeInfo(
		process.getHandle(),
		modBase,
		memberID,
		TI_GET_OFFSET,
		&offset);

	DWORD childAddress = address + offset;

	valueBuilder << TEXT("  ") << std::hex << std::uppercase << std::setfill(TEXT('0')) << std::setw(8) << childAddress << std::dec;

	// 値を出力する
	if (IsSimpleType(process, memberTypeID, modBase) == TRUE)
	{
		valueBuilder << TEXT("  ")
			<< GetTypeValue(
				process,
				memberTypeID,
				modBase,
				childAddress,
				pData + offset);
	}

	return TRUE;
}

// char型の文字をコンソールに出力可能な文字に変換する
// chが表示不可能な文字の場合、問号を返す
// それ以外の場合はchをそのまま返す
// 0x1E未満および0x7Fを超える値は表示できない
char ConvertToSafeChar(char ch)
{
	if (ch < 0x1E || ch > 0x7F)
	{
		return '?';
	}

	return ch;
}

// wchar_t型の文字をコンソールに出力可能な文字に変換する
// 現在のコードページでchが表示できない場合、問号を返す
// それ以外の場合はchをそのまま返す
wchar_t ConvertToSafeWChar(wchar_t ch)
{
	if (ch < 0x1E)
	{
		return L'?';
	}

	char buffer[4];

	size_t convertedCount;
	wcstombs_s(&convertedCount, buffer, 4, &ch, 2);

	if (convertedCount == 0)
	{
		return L'?';
	}

	return ch;
}
