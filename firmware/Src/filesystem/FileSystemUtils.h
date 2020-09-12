/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef FILESYSTEMUTILS_H_
#define FILESYSTEMUTILS_H_


class FileSystemUtils {
public:
	FileSystemUtils();
	virtual ~FileSystemUtils();

	void copy(char* dest, const char* source, int length);
	void addNumber(char* name, int offset, int number);
	int strlen(const char *string);
	int copy_string(char *target, const char *source);
	int printFloat(char *target, float f);
	int printInt(char *target, int i);
	int getPositionOfEqual(const char *line);
	void getKey(const char * line, char* key);
	int toInt(const char *str);
	float toFloat(const char *str);
	void getValue(char * line, char *value);
	void getFloatValue(char * line, char *value);
	void getTextValue(char * line, char *value);
	int str_cmp(const char*s1, const char*s2);
	int getLine(char* file, char* line);
	void  copyFloat(float* source, float* dest, int n);
	int getPositionOfSlash(const char *line);
	int getPositionOfPeriod(const char *line);
	float stof(const char* s, int &charRead);
	int strcmp(const char *s1, const char *s2);

	bool isNumber(char c) {
	    return (c >= '0' && c <= '9') || c == '.' || c == '-';
	}

	bool isSeparator(char c) {
	    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ;
	}


protected:
    char fullName[40];

};

#endif /* FILESYSTEMUTILS_H_ */
