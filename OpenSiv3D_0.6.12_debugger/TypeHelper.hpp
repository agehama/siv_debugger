#pragma once

enum BaseTypeEnum {
	btNoType = 0,
	btVoid = 1,
	btChar = 2,
	btWChar = 3,
	btInt = 6,
	btUInt = 7,
	btFloat = 8,
	btBCD = 9,
	btBool = 10,
	btLong = 13,
	btULong = 14,
	btCurrency = 25,
	btDate = 26,
	btVariant = 27,
	btComplex = 28,
	btBit = 29,
	btBSTR = 30,
	btHresult = 31
};

enum SymTagEnum {
	SymTagNull,
	SymTagExe,
	SymTagCompiland,
	SymTagCompilandDetails,
	SymTagCompilandEnv,
	SymTagFunction,			// 関数
	SymTagBlock,
	SymTagData,				// 変数
	SymTagAnnotation,
	SymTagLabel,
	SymTagPublicSymbol,
	SymTagUDT,				// ユーザー定義型（例：struct、class、union）
	SymTagEnum,				// 列挙型
	SymTagFunctionType,		// 関数型
	SymTagPointerType,		// ポインタ型
	SymTagArrayType,		// 配列型
	SymTagBaseType,			// 基本型
	SymTagTypedef,			// typedef型
	SymTagBaseClass,		// 基底クラス
	SymTagFriend,			// フレンド型
	SymTagFunctionArgType,	// 関数引数型
	SymTagFuncDebugStart,
	SymTagFuncDebugEnd,
	SymTagUsingNamespace,
	SymTagVTableShape,
	SymTagVTable,
	SymTagCustom,
	SymTagThunk,
	SymTagCustomType,
	SymTagManagedType,
	SymTagDimension
};

enum CBaseTypeEnum {
	cbtNone,
	cbtVoid,
	cbtBool,
	cbtChar,
	cbtUChar,
	cbtWChar,
	cbtShort,
	cbtUShort,
	cbtInt,
	cbtUInt,
	cbtLong,
	cbtULong,
	cbtLongLong,
	cbtULongLong,
	cbtFloat,
	cbtDouble,
	cbtEnd,
};
