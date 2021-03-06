#include "Defines.h"
#include <Wire.h>

short fileStartAddresses[maxFileHeaders] = { FILE_PARTITION_LOWER_BOUND };
short fileEndAddresses[maxFileHeaders] = { FILE_PARTITION_LOWER_BOUND };

int fileArrayIndex = 0;
short fileCount = 0;

void InitHFS() {

    Serial.begin(9600);
    InitIO();

    Serial.println("\nInitializing your TinyHFS...");

    /* The first byte in memory will indicate the number of files stored in the system.
     * This will be referred to as the file count byte.
     */

    fileCount = (short) readByte(EEPROM_ADDRESS, 0);

    /* The following code will obtain the start and end addresses of all files and their
     * corresponding parent folders
     */

    int fileIndex = FILE_HEADER_PARTITION_LOWER_BOUND + FILE_NAME_SIZE;
    short fileStartAddress = 0;
    short fileEndAddress = 0;

    for (int i = 0; i < fileCount; i++) {

        fileStartAddress = assembleShort(readByte(EEPROM_ADDRESS, fileIndex), readByte(EEPROM_ADDRESS, fileIndex + 1));

        fileEndAddress = assembleShort(readByte(EEPROM_ADDRESS, fileIndex + 2),
            readByte(EEPROM_ADDRESS, fileIndex + 3));

        fileIndex += fileHeaderSize;

        // Do nothing if empty space is read
        if (!(fileStartAddress || fileEndAddress)) {

            i--;
            continue;

        }

        fileStartAddresses[i] = fileStartAddress;
        fileEndAddresses[i] = fileEndAddress;

    }

    qsort(fileStartAddresses, fileCount, sizeof(short), compVals);
    qsort(fileEndAddresses, fileCount, sizeof(short), compVals);

    updateCurrentDirectory(ROOT_ADDRESS);

    Serial.println("\nWelcome to your TinyHFS!");
    Serial.println("\nIf you are new to the system or would like a refresher, enter \'help\' at any time.");
    Serial.println("If this is your first time using TinyHFS, please format the system before use.");

}

void writeFile(char file_text[], short file_size, short parentStartAddress, char *fileName) {

    double timeStart = millis() / 1000.0;

    short fileStartAddress = (fileCount == 0 ? FILE_PARTITION_LOWER_BOUND : fileEndAddresses[fileCount - 1] + 1);
    Serial.println("\nFILE WRITE INITIATED");

    if (fileStartAddress + file_size - 1 > FILE_PARTITION_UPPER_BOUND
        || !createFileHeader(fileStartAddress, parentStartAddress, file_size, fileName)) {

        Serial.print(
            "\nERROR: File space full; cannot save file. Try deleting some files, or organizing memory with the '");
        Serial.print(organize_mem);
        Serial.println("' command.");

        return;

    }

    for (int i = fileStartAddress; i <= fileStartAddress + file_size - 1; i++)
        writeByte(EEPROM_ADDRESS, i, file_text[i - fileStartAddress]);

    double timeEnd = millis() / 1000.0;

    Serial.print("\nFILE WRITTEN\n\nFILE LENGTH: ");
    Serial.print(file_size);
    Serial.print(" BYTES\n");
    Serial.println("\nFINISHED IN ");
    Serial.print(timeEnd - timeStart);
    Serial.print(" s\n");

    fileStartAddresses[fileCount] = fileStartAddress;
    fileEndAddresses[fileCount] = fileStartAddress + file_size - 1;

    writeByte(EEPROM_ADDRESS, 0, (byte)(++fileCount));

}

void readFile(short fileStartAddress, short fileEndAddress) {

    short fileSize = fileEndAddress - fileStartAddress + 1;
    unsigned char str_bytes[fileSize];

    Serial.println("\nFILE READ INITIATED");

    double timeStart = millis() / 1000.0;

    // Read the data byte-by-byte and store in str_bytes
    for (int i = fileStartAddress; i <= fileEndAddress; i++)
        str_bytes[i - fileStartAddress] = readByte(EEPROM_ADDRESS, i);

    double timeEnd = millis() / 1000.0;

    Serial.println("\n*** File Contents ***");

    // Print out the read data
    for (int i = 0; i < fileSize; i++)
        Serial.print((char) str_bytes[i]);

    Serial.println("\n*********************");

    Serial.print("\nFILE LENGTH: ");
    Serial.print(fileSize);
    Serial.print(" BYTES\n");
    Serial.println("\nFINISHED IN");
    Serial.print(timeEnd - timeStart);
    Serial.print(" s\n");

}

void createFolder(short parentStartAddress) {

    byte parentStartAddressLow = getLowByte(parentStartAddress);
    byte parentStartAddressHigh = getHighByte(parentStartAddress);

    char *folderName = getName(FOLDER_NAME_SIZE);

    if (!folderName) {

        return;

    }

    if (findStartAddressFromName(folderName, "folder")) {

        Serial.println("\nERROR: This folder already exists.");
        return;

    }

    short folderStartAddress = 0;
    int spaceCount = 0;

    for (int j = FOLDER_PARTITION_LOWER_BOUND; j <= FOLDER_PARTITION_UPPER_BOUND; j++) {

        spaceCount = (readByte(EEPROM_ADDRESS, j) == 0 ? spaceCount + 1 : 0);

        if (spaceCount == folderSize) {

            folderStartAddress = j - folderSize + 1;
            break;

        }

    }

    if (!folderStartAddress) {

        Serial.println("\nERROR: Folder space full; cannot create folder.");
        Serial.println("Please delete at least one folder if you wish to create a new one.");

        return;

    }

    byte folderStartAddressLow = getLowByte(folderStartAddress);
    byte folderStartAddressHigh = getHighByte(folderStartAddress);

    for (int i = 0; i < FOLDER_NAME_SIZE; i++)
        writeByte(EEPROM_ADDRESS, folderStartAddress + i, folderName[i]);

    // Write start address of folder
    writeByte(EEPROM_ADDRESS, folderStartAddress + FOLDER_NAME_SIZE, folderStartAddressHigh);
    writeByte(EEPROM_ADDRESS, folderStartAddress + FOLDER_NAME_SIZE + 1, folderStartAddressLow);

    // Write start address of parent folder
    writeByte(EEPROM_ADDRESS, folderStartAddress + FOLDER_NAME_SIZE + 2, parentStartAddressHigh);
    writeByte(EEPROM_ADDRESS, folderStartAddress + FOLDER_NAME_SIZE + 3, parentStartAddressLow);

    Serial.println("...done");

}

void rename(short startAddress, char *nameNew, short nameSize) {

    for (int i = startAddress; i < startAddress + nameSize; i++)
        writeByte(EEPROM_ADDRESS, i, nameNew[i - startAddress]);

    Serial.println("...done");
}

/**
 * Function : getName
 * 
 * Returns a character array entered by the user.
 * 
 * @param maxNameSize name length limit
 */
char *getName(int maxNameSize) {

    char *name = (char *) malloc(sizeof(char) * maxNameSize);
    char received = 0;
    String inData;

    Serial.flush();

    while (1) {   //loops until input is received

        while (Serial.available() > 0) {

            received = Serial.read();

            inData += received;

            // Process string when new line character is received
            if (received == '\n') {

                // Work out length of string
                int str_len = inData.length() - 1;

                if (str_len > maxNameSize) {

                    Serial.println("\nERROR: Name too long; please try again.");
                    free(name);
                    return 0;

                }

                // Split string into an array of char arrays, each one byte long
                for (int j = 0; j < str_len; j++)
                    name[j] = inData[j];

                for (int j = str_len; j < maxNameSize; j++)
                    name[j] = 0;

                return name;

            }

        }

    }

}

// Returns 1 if successful, else 0
int createFileHeader(short fileStartAddress, short parentStartAddress, short fileSize, char *fileName) {

    short fileEndAddress = fileStartAddress + fileSize - 1;

    byte fileStartAddressLow = getLowByte(fileStartAddress);
    byte fileStartAddressHigh = getHighByte(fileStartAddress);
    byte fileEndAddressLow = getLowByte(fileEndAddress);
    byte fileEndAddressHigh = getHighByte(fileEndAddress);

    byte parentStartAddressLow = getLowByte(parentStartAddress);
    byte parentStartAddressHigh = getHighByte(parentStartAddress);

    short fileHeaderStartAddress = 0;
    int spaceCount = 0;

    for (int j = FILE_HEADER_PARTITION_LOWER_BOUND; j <= FILE_HEADER_PARTITION_UPPER_BOUND; j++) {

        spaceCount = (readByte(EEPROM_ADDRESS, j) == 0 ? spaceCount + 1 : 0);

        if (spaceCount == fileHeaderSize) {

            fileHeaderStartAddress = j - fileHeaderSize + 1;
            break;

        }

    }

    if (!fileHeaderStartAddress) {

        Serial.print(
            "\nERROR: File space full; cannot save file. Try deleting some files, or organizing memory with the '");
        Serial.print(organize_mem);
        Serial.println("' command.");

        return 0;

    }

    for (int i = 0; i < FILE_NAME_SIZE; i++)
        writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + i, fileName[i]);

    // Write start address of file
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE, fileStartAddressHigh);
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE + 1, fileStartAddressLow);

    // Write end address of file
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE + 2, fileEndAddressHigh);
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE + 3, fileEndAddressLow);

    // Write start address of parent folder
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE + 4, parentStartAddressHigh);
    writeByte(EEPROM_ADDRESS, fileHeaderStartAddress + FILE_NAME_SIZE + 5, parentStartAddressLow);

    return 1;

}

void copyFile(short srcStartAddress, short srcEndAddress, short destStartAddress, char *fileName) {

    double timeStart = millis() / 1000.0;

    int file_size = srcEndAddress - srcStartAddress + 1;
    short fileStartAddress = (fileCount == 0 ? FILE_PARTITION_LOWER_BOUND : fileEndAddresses[fileCount - 1] + 1);
    Serial.println("\nFILE COPY INITIATED");

    if (fileStartAddress + file_size - 1 > FILE_PARTITION_UPPER_BOUND
        || !createFileHeader(fileStartAddress, destStartAddress, file_size, fileName)) {

        Serial.println("\nERROR: File space full; cannot save file");

        return;

    }

    for (int i = fileStartAddress; i <= fileStartAddress + file_size - 1; i++)
        writeByte(EEPROM_ADDRESS, i, readByte(EEPROM_ADDRESS, srcStartAddress + i - fileStartAddress));

    double timeEnd = millis() / 1000.0;

    Serial.print("\nFILE COPIED\n\nFILE LENGTH: ");
    Serial.print(file_size);
    Serial.print(" BYTES\n");
    Serial.println("\nFINISHED IN");
    Serial.print(timeEnd - timeStart);
    Serial.print(" s\n");

    fileStartAddresses[fileCount] = fileStartAddress;
    fileEndAddresses[fileCount] = fileStartAddress + file_size - 1;

    writeByte(EEPROM_ADDRESS, 0, (byte)(++fileCount));

}

void moveFile() {

}

void deleteFile(short fileHeaderStartAddress) {

    short fileAddressesLocInHeader = fileHeaderStartAddress + FILE_NAME_SIZE;

    byte fileStartAddressHighByte = readByte(EEPROM_ADDRESS, fileAddressesLocInHeader);
    byte fileStartAddressLowByte = readByte(EEPROM_ADDRESS, fileAddressesLocInHeader + 1);
    short fileStartAddress = assembleShort(fileStartAddressHighByte, fileStartAddressLowByte);

    byte fileEndAddressHighByte = readByte(EEPROM_ADDRESS, fileAddressesLocInHeader + 2);
    byte fileEndAddressLowByte = readByte(EEPROM_ADDRESS, fileAddressesLocInHeader + 3);
    short fileEndAddress = assembleShort(fileEndAddressHighByte, fileEndAddressLowByte);

    for (int i = fileStartAddress; i <= fileEndAddress; i++)
        writeByte(EEPROM_ADDRESS, i, 0);

    /*
     * Check if the deleted file is located in the middle of the file array. If it is, then shift all the remaining
     * files over by one.
     */
    int mustShift = 0;

    for (int i = 0; i < fileCount; i++) {

        if (mustShift)
            fileStartAddresses[i - 1] = fileStartAddresses[i];

        if (fileStartAddresses[i] == fileStartAddress)
            mustShift = 1;

    }

    fileStartAddresses[--fileCount] = 0;

    writeByte(EEPROM_ADDRESS, 0, (byte)(fileCount));

    // Overwrite the file header with null characters
    for (int i = fileHeaderStartAddress; i < fileHeaderStartAddress + fileHeaderSize; i++)
        writeByte(EEPROM_ADDRESS, i, 0);

}

void deleteFolder(short folderStartAddress) {

    int folderEndAddress = folderStartAddress + folderSize - 1;

    // Prohibit user from deleting root folder
    if (folderStartAddress == ROOT_ADDRESS) {

        Serial.println("\nERROR: Cannot delete root folder");
        return;

    }

    // Overwrite the folder with null characters
    for (int i = folderStartAddress; i <= folderEndAddress; i++)
        writeByte(EEPROM_ADDRESS, i, 0);

    /*
     * Scan the parent folder start address attribute of all the file headers (15th and 16th addresses),
     * then delete the file headers and files whose parent folder start address corresponds to the folder
     * being deleted.
     */
    for (int i = FILE_HEADER_PARTITION_LOWER_BOUND + FILE_NAME_SIZE + 4; i <= FILE_HEADER_PARTITION_UPPER_BOUND; i +=
        fileHeaderSize) {

        char parentStartAddressCurrHigh = readByte(EEPROM_ADDRESS, i);
        char parentStartAddressCurrLow = readByte(EEPROM_ADDRESS, i + 1);
        short parentStartAddressCurr = assembleShort(parentStartAddressCurrHigh, parentStartAddressCurrLow);

        if (folderStartAddress == parentStartAddressCurr) {

            short fileHeaderStartAddressCurr = i - FILE_NAME_SIZE - 4;

            deleteFile(fileHeaderStartAddressCurr);

        }

    }

}

// Clears the EEPROM by overwriting all data with the null character (takes approximately 144 seconds)
void format() {

    double timeStart = millis() / 1000.0;
    Serial.println("\nEEPROM FORMAT INITIATED");

    for (int i = MIN_ADDRESS; i <= MAX_ADDRESS; i++)
        writeByte(EEPROM_ADDRESS, i, 0);

    double timeEnd = millis() / 1000.0;

    Serial.print("\nEEPROM FORMATTED\n\n");
    Serial.println("FINISHED IN ");
    Serial.print(timeEnd - timeStart);
    Serial.print(" s\n");

    InitHFS();

}

/**
 *   Function : organizeMemory
 *
 *   When writing a file to the EEPROM, the file written is adjacent to the greatest address that is occupied.
 *   This creates an efficiency problem. When a file is deleted somewhere in the middle of occupied memory,
 *   free space is created but cannot be accessed. This function corrects this by shifting every file to the
 *   left according to the number bytes of free space. This allows us to effectively make use of previously
 *   occupied space.
 */
void organizeMemory() {

    short shiftHowMany;
    short fileSize;
    int needsShifting = 1;

    qsort(fileStartAddresses, fileCount, sizeof(short), compVals);
    qsort(fileEndAddresses, fileCount, sizeof(short), compVals);

    Serial.println("\nOrganizing memory...");

    shiftHowMany = fileStartAddresses[0] - FILE_PARTITION_LOWER_BOUND;

    if (shiftHowMany <= 0)
        needsShifting = 0;

    fileSize = fileEndAddresses[0] - fileStartAddresses[0] + 1;

    if (needsShifting) {

        for (int j = 0; j < fileSize; j++) {

            writeByte(EEPROM_ADDRESS, fileStartAddresses[0] - shiftHowMany + j,
                readByte(EEPROM_ADDRESS, fileStartAddresses[0] + j));

        }

        for (int i = FILE_HEADER_PARTITION_LOWER_BOUND + FILE_NAME_SIZE; i <= FILE_HEADER_PARTITION_UPPER_BOUND; i +=
            fileHeaderSize) {

            short readAddress = assembleShort(readByte(EEPROM_ADDRESS, i), readByte(EEPROM_ADDRESS, i + 1));

            if (readAddress == fileStartAddresses[0]) {

                writeByte(EEPROM_ADDRESS, i, getHighByte(fileStartAddresses[0] - shiftHowMany));
                writeByte(EEPROM_ADDRESS, i + 1, getLowByte(fileStartAddresses[0] - shiftHowMany));
                writeByte(EEPROM_ADDRESS, i + 2, getHighByte(fileEndAddresses[0] - shiftHowMany));
                writeByte(EEPROM_ADDRESS, i + 3, getLowByte(fileEndAddresses[0] - shiftHowMany));
                break;

            }

        }

        fileStartAddresses[0] -= shiftHowMany;
        fileEndAddresses[0] -= shiftHowMany;

    }

    needsShifting = 1;

    for (int i = 1; i < fileCount; i++) {

        shiftHowMany = fileStartAddresses[i] - fileEndAddresses[i - 1] - 1;

        if (shiftHowMany <= 0)
            needsShifting = 0;

        fileSize = fileEndAddresses[i] - fileStartAddresses[i] + 1;

        if (needsShifting) {

            for (int j = 0; j < fileSize; j++)
                writeByte(EEPROM_ADDRESS, fileStartAddresses[i] - shiftHowMany + j,
                    readByte(EEPROM_ADDRESS, fileStartAddresses[i] + j));

            for (int k = FILE_HEADER_PARTITION_LOWER_BOUND + FILE_NAME_SIZE; k <= FILE_HEADER_PARTITION_UPPER_BOUND;
                k += fileHeaderSize) {

                short readAddress = assembleShort(readByte(EEPROM_ADDRESS, k), readByte(EEPROM_ADDRESS, k + 1));

                if (readAddress == fileStartAddresses[i]) {

                    writeByte(EEPROM_ADDRESS, k, getHighByte(fileStartAddresses[i] - shiftHowMany));
                    writeByte(EEPROM_ADDRESS, k + 1, getLowByte(fileStartAddresses[i] - shiftHowMany));
                    writeByte(EEPROM_ADDRESS, k + 2, getHighByte(fileEndAddresses[i] - shiftHowMany));
                    writeByte(EEPROM_ADDRESS, k + 3, getLowByte(fileEndAddresses[i] - shiftHowMany));

                    break;

                }

            }

            fileStartAddresses[i] -= shiftHowMany;
            fileEndAddresses[i] -= shiftHowMany;

        }

        needsShifting = 1;

    }

}

// For use with qsort
int compVals(const void* a, const void* b) {

    return (*(short *) a - *(short *) b);

}

byte getLowByte(short num) {

    return (byte)(num & 0xff);

}

byte getHighByte(short num) {

    return (byte)(num >> 8 & 0xff);

}

// Concatenates a high and low byte into a short
short assembleShort(byte high, byte low) {

    return (short) (low | high << 8);

}
