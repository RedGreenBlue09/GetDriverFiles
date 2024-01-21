#include <inttypes.h>
#include <stdio.h>

#include <Windows.h>
#include <setupapi.h>

#include "GuardedMalloc.h"
#include "Tree234.h"

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

typedef struct {
	int32_t Id;
	char* Path;
} disk_properties;

static int DiskIdCompare(disk_properties* A, disk_properties* B) {
	return (A->Id > B->Id) - (A->Id < B->Id);
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

			uint32_t FileNameLength = 0; // '\0' included
			if (
				!SetupGetStringFieldA(
					&InfContext,
					1,
					NULL,
					0,
					&FileNameLength
				)
			)
				continue; // TODO: Error message

			char* psFileName = malloc_guarded(FileNameLength * sizeof(char));
			SetupGetStringFieldA(&InfContext, 1, psFileName, FileNameLength, NULL);

			// From the docs: "Windows assumes that the catalog file is in the same location as the INF file."
			puts(psFileName);
			free(psFileName);

		}

	};

	// Get disk paths
	// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-sourcedisksnames-section
	
	char* asSourceDisksNamesVariants[] = {
		"SourceDisksNames",
		"SourceDisksNames.X86",
		"SourceDisksNames.IA64",
		"SourceDisksNames.AMD64",
		"SourceDisksNames.ARM",
		"SourceDisksNames.ARM64",
	};
	tree234* pDisksPropTree = newtree234(DiskIdCompare);
	for (size_t i = 0; i < static_arrlen(asSourceDisksNamesVariants); ++i) {

		// Assuming there are no repeated entry.
		INFCONTEXT InfContext;
		if (
			SetupFindFirstLineA(
				hInf,
				asSourceDisksNamesVariants[i],
				NULL,
				&InfContext
			)
		) {

			int32_t RemainingLines = SetupGetLineCountA(hInf, asSourceDisksNamesVariants[i]);
			if (RemainingLines == -1)
				continue;

			while (RemainingLines > 0) {
				
				disk_properties* pDiskProperties = malloc_guarded(sizeof(*pDiskProperties));
				if (!SetupGetIntField(&InfContext, 0, &pDiskProperties->Id)) {
					// TODO: Error message
					goto NextLine0;
				}

				// Skip overlapped entries
				if (find234(pDisksPropTree, pDiskProperties, NULL))
					goto NextLine0;

				uint32_t PathLength = 0;
				if (
					SetupGetStringFieldA(
						&InfContext,
						4,
						NULL,
						0,
						&PathLength
					)
				) {
					if (PathLength == 0)
						// Don't malloc nothing.
						pDiskProperties->Path = NULL;
					else {
						pDiskProperties->Path = malloc_guarded(PathLength * sizeof(*pDiskProperties->Path));
						SetupGetStringFieldA(
							&InfContext,
							4,
							pDiskProperties->Path,
							PathLength,
							NULL
						);
						// Luckily, any trailing backslashes are removed by this function.
					}
				} else {
					pDiskProperties->Path = NULL;
				}
				add234(pDisksPropTree, pDiskProperties);

				NextLine0:
				SetupFindNextLine(&InfContext, &InfContext);
				--RemainingLines;

			};

		}

	};

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
				continue;

			while (RemainingLines > 0) {

				// Get file name length

				uint32_t FileNameLength = 0;
				if (
					!SetupGetStringFieldA(
						&InfContext,
						0,
						NULL,
						0,
						&FileNameLength
					)
				) {
					// TODO: Error message
					goto NextLine1;
				}
				FileNameLength -= 1;

				// Get corresponding disk path

				uint32_t DiskId = 0;
				if (!SetupGetIntField(&InfContext, 1, &DiskId)) {
					// TODO: Error message
					goto NextLine1;
				}

				disk_properties* pDiskProperties = find234(
					pDisksPropTree,
					&(disk_properties){ DiskId, NULL },
					NULL
				);

				BOOL bHaveDiskPath;
				size_t DiskPathLength = 0;
				if (pDiskProperties) {
					if (pDiskProperties->Path) {
						DiskPathLength = strlen(pDiskProperties->Path);
						bHaveDiskPath = TRUE;
					}
					else
						bHaveDiskPath = FALSE;
				} else {
					// TODO: Error message
					bHaveDiskPath = FALSE;
				}

				// Get sub dir length

				uint32_t SubdirLength = 0;
				BOOL bHaveSubdir = SetupGetStringFieldA(
					&InfContext,
					2,
					NULL,
					0,
					&SubdirLength
				);
				SubdirLength -= 1;
				bHaveSubdir &= SubdirLength > 0; // Handle empty sub dir "0,,"

				// Combine all parts

				size_t FullPathLength = 0;
				if (bHaveDiskPath)
					FullPathLength += DiskPathLength + 1; // Add '\\'
				if (bHaveSubdir)
					FullPathLength += SubdirLength + 1; // Add '\\'
				FullPathLength += FileNameLength + 1; // Add '\0'
				// Note: Always leave a character for SetupGetStringFieldA to put '\0'

				char* sFullPathName = malloc_guarded(FullPathLength * sizeof(*sFullPathName));
				char* pFullPathName2 = sFullPathName;

				if (bHaveDiskPath) {
					memcpy(pFullPathName2, pDiskProperties->Path, DiskPathLength);
					pFullPathName2 += DiskPathLength;
					*pFullPathName2++ = '\\';
				}

				if (bHaveSubdir) {
					SetupGetStringFieldA(
						&InfContext,
						2,
						pFullPathName2,
						SubdirLength + 1,
						NULL
					);
					pFullPathName2 += SubdirLength;
					*pFullPathName2++ = '\\';
				}

				SetupGetStringFieldA(
					&InfContext,
					0,
					pFullPathName2,
					FileNameLength + 1,
					NULL
				);
				pFullPathName2 += FileNameLength;

				// Print

				puts(sFullPathName);
				free(sFullPathName);

				NextLine1:
				SetupFindNextLine(&InfContext, &InfContext);
				--RemainingLines;

			};

		}

	};

	for (
		disk_properties* p = delpos234(pDisksPropTree, 0);
		p != NULL;
		p = delpos234(pDisksPropTree, 0)
	) {
		if (p->Path) free(p->Path);
		free(p);
	}
	freetree234(pDisksPropTree);

	return 0;
}
