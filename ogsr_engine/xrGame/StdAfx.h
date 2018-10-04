#pragma once

#pragma warning(disable:4995)
#pragma warning(disable:4577)
#include "../xr_3da/stdafx.h"
#pragma warning(default:4995)
#pragma warning( 4 : 4018 )
#pragma warning( 4 : 4244 )
#pragma warning(disable:4505)

#include "..\xr_3da\ai_script_space.h" //KRodin: ����� ��� ��� ������� � �������� �������� � ����� �����.

// this include MUST be here, since smart_cast is used >1800 times in the project
#include <smart_cast.h>

#define READ_IF_EXISTS(ltx,method,section,name,default_value)\
	((ltx->line_exist(section,name)) ? (ltx->method(section,name)) : (default_value))

#define THROW VERIFY
#define THROW2 VERIFY2
#define THROW3 VERIFY3

#include "../xr_3da/gamefont.h"
#include "../xr_3da/xr_object.h"
#include "../xr_3da/igame_level.h"

#ifndef DEBUG
#	define MASTER_GOLD
#endif
