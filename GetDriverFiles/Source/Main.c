#include <inttypes.h>
#include <stdio.h>

#include <Windows.h>
#include <setupapi.h>

#include "DynamicArray.h"
#include "GuardedMalloc.h"

#define static_arrlen(X) (sizeof(X) / sizeof(*X))
#define cast(T, X) ((T)(X))

static char* GetSystemErrorMessage(uint32_t Win32Error) {
	char* sErrorMessage = NULL;
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		HRESULT_FROM_WIN32(Win32Error),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&sErrorMessage,
		0,
		NULL
	);
	return sErrorMessage;
}

int main(int argc, char** argv) {

	if (argc < 2) {
		fprintf(stderr, "No INF file specified.\n");
		return 1;
	}

	unsigned int ErrorLine; // Currently unused
	HINF hInf = SetupOpenInfFileA(
		argv[1],
		NULL,
		INF_STYLE_WIN4,
		&ErrorLine
	);
	if (hInf == INVALID_HANDLE_VALUE) {
		char* sErrorMessage = GetSystemErrorMessage(GetLastError());
		if (sErrorMessage) {
			printf(
				"Unable to open the file '%s':\n"
				"%s",
				argv[1],
				sErrorMessage
			);
			LocalFree(sErrorMessage);
		} else {
			printf(
				"Unable to open the file '%s':\n"
				"An unknown error has occured.",
				argv[1]
			);
		}
		return 1;
	}

	// A driver package contains:
	//  + INF files (the user already knows it)
	//  + Catalog files
	//  + Driver files (.sys) and other files
	//    I think these 2 are all included in [SourceDisksFiles],
	//    They're called "source files".
	// 
	// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/components-of-a-driver-package

	// Get catalog files
	// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-version-section

	char* asCatalogFileVariants[] = {
		"CatalogFile",
		"CatalogFile.NT",
		"CatalogFile.NTX86",
		"CatalogFile.NTIA64",
		"CatalogFile.NTAMD64",
		"CatalogFile.NTARM",
		"CatalogFile.NTARM64",
	};
	dynamic_array CatalogFileList;
	DaInitialize(&CatalogFileList, sizeof(char*));
	for (size_t i = 0; i < static_arrlen(asCatalogFileVariants); ++i) {

		// Assuming there are no repeated entry.
		INFCONTEXT InfContext;
		if (
			SetupFindFirstLineA(
				hInf,
				"Version",
				asCatalogFileVariants[i],
				&InfContext
			)
		) {

			DaResize(&CatalogFileList, CatalogFileList.UsedSize + 1);
			char** psFileName = (char**)CatalogFileList.pData + (CatalogFileList.UsedSize - 1);

			uint32_t FileNameLength = 0;
			if (
				!SetupGetStringFieldA(
					&InfContext,
					1,
					NULL,
					0,
					&FileNameLength
				)
			)
				continue; // TODO: Error handling

			*psFileName = malloc_guarded(FileNameLength * sizeof(char));
			SetupGetStringFieldA(&InfContext, 1, *psFileName, FileNameLength, NULL);

		}

	};

	// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-version-section
	// From the docs: "Windows assumes that the catalog file is in the same location as the INF file."

	char** asCatalogFileList = (char**)CatalogFileList.pData;
	for (size_t i = 0; i < CatalogFileList.UsedSize; ++i)
		printf("%s\n", asCatalogFileList[i]);

	// Get source files
	// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-sourcedisksfiles-section

	char* asSourceDisksFilesVariants[] = {
		"SourceDisksFiles",
		"SourceDisksFiles.X86",
		"SourceDisksFiles.IA64",
		"SourceDisksFiles.AMD64",
		"SourceDisksFiles.ARM",
		"SourceDisksFiles.ARM64",
	};
	dynamic_array SourceFileList;
	DaInitialize(&SourceFileList, sizeof(char*));
	for (size_t i = 0; i < static_arrlen(asSourceDisksFilesVariants); ++i) {

		// Assuming there are no repeated entry.
		INFCONTEXT InfContext;
		if (
			SetupFindFirstLineA(
				hInf,
				asSourceDisksFilesVariants[i],
				NULL,
				&InfContext
			)
		) {
			int32_t RemainingLines = SetupGetLineCountA(hInf, asSourceDisksFilesVariants[i]);
			if (RemainingLines == -1)
				continue; // TODO: Error handling

			while (RemainingLines > 0) {

				DaResize(&SourceFileList, SourceFileList.UsedSize + 1);
				char** psFilePath = (char**)SourceFileList.pData + (SourceFileList.UsedSize - 1);

				uint32_t FileNameLength = 0;
				if (
					!SetupGetStringFieldA(
						&InfContext,
						0, NULL,
						0,
						&FileNameLength
					)
				)
					continue; // TODO: Error handling
				uint32_t SubdirLength = 0;
				BOOL bHaveSubdir = SetupGetStringFieldA(
					&InfContext,
					2,
					NULL,
					0,
					&SubdirLength
				);

				bHaveSubdir &= SubdirLength > 1; // Handle empty sub dir "0,,"
				// Remove '\0' and add '\\'
				size_t SubdirSlashLength = bHaveSubdir ? ((size_t)SubdirLength - 1 + 1) : 0;
				*psFilePath = malloc_guarded((SubdirSlashLength + FileNameLength) * sizeof(char));

				// From the docs:
				// "If this value is omitted from an entry, the named source file
				// is assumed to be in the path directory that was specified in
				// the SourceDisksFiles section for the given disk or,
				// if no path directory was specified, in the installation root."

				// Disks can have custom paths that the [SourceDisksFiles]'s
				// path field is related to (defined in [SourceDisksNames],
				// so this path is not always relative to the installation root.
				// CAUTION: This case is not covered in this program as it's not required.

				if (bHaveSubdir) {
					SetupGetStringFieldA(
						&InfContext,
						2,
						*psFilePath,
						SubdirLength,
						NULL
					);
					(*psFilePath)[SubdirSlashLength - 1] = '\\';
				}
				SetupGetStringFieldA(
					&InfContext,
					0,
					*psFilePath + SubdirSlashLength,
					FileNameLength,
					NULL
				);

				SetupFindNextLine(&InfContext, &InfContext);
				--RemainingLines;

			};
		}

	};

	char** asSourceFileList = (char**)SourceFileList.pData;
	for (size_t i = 0; i < SourceFileList.UsedSize; ++i)
		printf("%s\n", asSourceFileList[i]);

	return 0;
}
