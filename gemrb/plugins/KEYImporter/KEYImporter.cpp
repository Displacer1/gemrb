/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */

#include "../../includes/win32def.h"
#include "KEYImporter.h"
#include "../../includes/globals.h"
#include "../Core/FileStream.h"
#include "../Core/Interface.h"
#include "../Core/ArchiveImporter.h"
#include "../Core/ResourceDesc.h"
#ifndef WIN32
#include <unistd.h>
#endif

#define SHARED_OVERRIDE "shared"

KEYImporter::KEYImporter(void)
{
}

KEYImporter::~KEYImporter(void)
{
	for (unsigned int i = 0; i < biffiles.size(); i++) {
		free( biffiles[i].name );
	}
}

static bool exists(char *file)
{
	FILE *f = fopen( file, "rb" );
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

static void FindBIF(BIFEntry *entry)
{
	entry->cd = 0;

	PathJoin( entry->path, core->GamePath, entry->name, NULL );
	ResolveFilePath(entry->path);
	if (exists(entry->path)) {
		entry->found = true;
		return;
	}

	PathJoin( entry->path, core->GamePath, entry->name, NULL );
	strcpy( entry->path + strlen( entry->path ) - 4, ".cbf" );
	ResolveFilePath(entry->path);
	if (exists(entry->path)) {
		entry->found = true;
		return;
	}

	if (core->GameOnCD) {
		char BasePath[_MAX_PATH];
		if (( entry->BIFLocator & ( 1 << 2 ) ) != 0) {
			strcpy( BasePath, core->CD[0] );
			entry->cd = 1;
		} else if (( entry->BIFLocator & ( 1 << 3 ) ) != 0) {
			strcpy( BasePath, core->CD[1] );
			entry->cd = 2;
		} else if (( entry->BIFLocator & ( 1 << 4 ) ) != 0) {
			strcpy( BasePath, core->CD[2] );
			entry->cd = 3;
		} else if (( entry->BIFLocator & ( 1 << 5 ) ) != 0) {
			strcpy( BasePath, core->CD[3] );
			entry->cd = 4;
		} else if (( entry->BIFLocator & ( 1 << 6 ) ) != 0) {
			strcpy( BasePath, core->CD[4] );
			entry->cd = 5;
		} else {
			printStatus( "ERROR", LIGHT_RED );
			printf( "Cannot find %s... Resource unavailable.\n",
					entry->name );
			entry->found = false;
			return;
		}
		PathJoin( entry->path, BasePath, entry->name, NULL );
		entry->found = false;
		return;
	}

	for (int i = 0; i < 6; i++) {
		PathJoin( entry->path, core->CD[i], entry->name, NULL );
		ResolveFilePath(entry->path);
		if (exists(entry->path)) {
			entry->found = true;
			return;
		}

		//Trying CBF Extension
		PathJoin( entry->path, core->CD[i], entry->name, NULL );
		strcpy( entry->path + strlen( entry->path ) - 4, ".cbf" );
		ResolveFilePath(entry->path);
		if (exists(entry->path)) {
			entry->found = true;
			return;
		}
	}

	printMessage( "KEYImporter", " ", WHITE );
	printf( "Cannot find %s...", entry->name );
	printStatus( "ERROR", LIGHT_RED );
	entry->found = false;
}

bool KEYImporter::Open(const char *resfile, const char *desc)
{
	description = desc;
	if (!core->IsAvailable( IE_BIF_CLASS_ID )) {
		printf( "[ERROR]\nAn Archive Plug-in is not Available\n" );
		return false;
	}
	unsigned int i;
	// NOTE: Interface::Init has already resolved resfile.
	printMessage( "KEYImporter", "Opening ", WHITE );
	printf( "%s...", resfile );
	FileStream* f = new FileStream();
	if (!f->Open( resfile )) {
		printStatus( "ERROR", LIGHT_RED );
		printMessage( "KEYImporter", "Cannot open Chitin.key\n", LIGHT_RED );
		textcolor( WHITE );
		delete( f );
		return false;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "KEYImporter", "Checking file type...", WHITE );
	char Signature[8];
	f->Read( Signature, 8 );
	if (strncmp( Signature, "KEY V1  ", 8 ) != 0) {
		printStatus( "ERROR", LIGHT_RED );
		printMessage( "KEYImporter", "File has an Invalid Signature.\n",
			LIGHT_RED );
		textcolor( WHITE );
		delete( f );
		return false;
	}
	printStatus( "OK", LIGHT_GREEN );
	printMessage( "KEYImporter", "Reading Resources...\n", WHITE );
	ieDword BifCount, ResCount, BifOffset, ResOffset;
	f->ReadDword( &BifCount );
	f->ReadDword( &ResCount );
	f->ReadDword( &BifOffset );
	f->ReadDword( &ResOffset );
	printMessage( "KEYImporter", " ", WHITE );
	printf( "BIF Files Count: %d (Starting at %d Bytes)\n", BifCount,
		BifOffset );
	printMessage( "KEYImporter", " ", WHITE );
	printf( "RES Count: %d (Starting at %d Bytes)\n", ResCount, ResOffset );
	f->Seek( BifOffset, GEM_STREAM_START );
	ieDword BifLen, ASCIIZOffset;
	ieWord ASCIIZLen;
	for (i = 0; i < BifCount; i++) {
		BIFEntry be;
		f->Seek( BifOffset + ( 12 * i ), GEM_STREAM_START );
		f->ReadDword( &BifLen );
		f->ReadDword( &ASCIIZOffset );
		f->ReadWord( &ASCIIZLen );
		f->ReadWord( &be.BIFLocator );
		be.name = ( char * ) malloc( ASCIIZLen );
		f->Seek( ASCIIZOffset, GEM_STREAM_START );
		f->Read( be.name, ASCIIZLen );
#ifndef WIN32
		for (int p = 0; p < ASCIIZLen; p++) {
			//some MAC versions use : as delimiter
			if (be.name[p] == '\\' || be.name[p] == ':')
				be.name[p] = PathDelimiter;
		}
		if (be.name[0] == PathDelimiter) {
			// totl has '\data\zcMHar.bif' in the key file, and the CaseSensitive
			// code breaks with that extra slash, so simple fix: remove it
			ASCIIZLen--;
			for (int p = 0; p < ASCIIZLen; p++)
				be.name[p] = be.name[p + 1];
			// (if you change this, try moving to ar9700 for testing)
		}
#endif
		FindBIF(&be);
		biffiles.push_back( be );
	}
	f->Seek( ResOffset, GEM_STREAM_START );
	resources.InitHashTable( ResCount < 17 ? 17 : ResCount );
	for (i = 0; i < ResCount; i++) {
		RESEntry re;
		f->ReadResRef( re.ResRef );
		f->ReadWord( &re.Type );
		f->ReadDword( &re.ResLocator );
		resources.SetAt( re.ResRef, re.Type, re.ResLocator );
	}
	printMessage( "KEYImporter", "Resources Loaded...", WHITE );
	printStatus( "OK", LIGHT_GREEN );
	delete( f );
	return true;
}

bool KEYImporter::HasResource(const char* resname, SClass_ID type, bool)
{
	unsigned int ResLocator;
	if (resources.Lookup( resname, type, ResLocator ))
		return true;
	return false;
}

bool KEYImporter::HasResource(const char* resname, std::vector<ResourceDesc> types, bool)
{
	unsigned int ResLocator;
	for (size_t j = 0; j < types.size(); j++) {
		if (resources.Lookup( resname, types[j].GetKeyType(), ResLocator )) {
			return true;
		}
	}
	return false;
}

static void FindBIFOnCD(BIFEntry *entry)
{
	ResolveFilePath(entry->path);
	if (exists(entry->path)) {
		entry->found = true;
		return;
	}

	core->WaitForDisc( entry->cd, core->CD[entry->cd-1] );
	ResolveFilePath(entry->path);
	if (exists(entry->path)) {
		entry->found = true;
		return;
	}

	//Trying CBF Extension
	strcpy( entry->path + strlen( entry->path ) - 4, ".cbf" );
	ResolveFilePath(entry->path);
	if (exists(entry->path)) {
		entry->found = true;
		return;
	}

	entry->found = false;
}

DataStream* KEYImporter::GetStream(const char *resname, ieWord type, bool silent)
{
	char path[_MAX_PATH];
	unsigned int ResLocator;

	if (type == 0)
		return NULL;
	if (resources.Lookup( resname, type, ResLocator )) {
		int bifnum = ( ResLocator & 0xFFF00000 ) >> 20;

		strcpy( path, biffiles[bifnum].path );
		if (core->GameOnCD)
			FindBIFOnCD(&biffiles[bifnum]);
		if (!biffiles[bifnum].found) {
			printf( "Cannot find %s... Resource unavailable.\n",
					biffiles[bifnum].name );
			return NULL;
		}

		ArchiveImporter* ai = ( ArchiveImporter* )
			core->GetInterface( IE_BIF_CLASS_ID );
		if (ai->OpenArchive( path ) == GEM_ERROR) {
			printf("Cannot open archive %s\n", path );
			core->FreeInterface( ai );
			return NULL;
		}
		DataStream* ret = ai->GetStream( ResLocator, type, silent );
		core->FreeInterface( ai );
		if (ret) {
			strnlwrcpy( ret->filename, resname, 8 );
			strcat( ret->filename, core->TypeExt( type ) );
			return ret;
		}
	}
	return NULL;
}

DataStream* KEYImporter::GetResource(const char* resname, SClass_ID type, bool silent)
{
	if (!strcmp(resname, "")) return NULL;

	//the word masking is a hack for synonyms, currently used for bcs==bs
	return GetStream(resname, type&0xFFFF, silent);
}

Resource* KEYImporter::GetResource(const char* resname, const std::vector<ResourceDesc> &types, bool silent)
{
	if (!strcmp(resname, "")) return NULL;

	for (size_t j = 0; j < types.size(); j++) {
		if (DataStream* ret = GetStream(resname, types[j].GetKeyType(), silent)) {
			if (Resource *res = types[j].Create(ret)) {
				return res;
			}
		}
	}
	return NULL;
}

#include "../../includes/plugindef.h"

GEMRB_PLUGIN(0x1DFDEF80, "KEY File Importer")
PLUGIN_CLASS(PLUGIN_RESOURCE_KEY, KEYImporter)
END_PLUGIN()
