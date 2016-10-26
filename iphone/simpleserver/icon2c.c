/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2016  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define ICONS_C "icons.c"
#define ICON2C_CONF "icon2c.conf"
#define ICON2C "icon2c"

FILE *out;

void write_bytes(FILE *file) {
    int pos;
    int first = 1;
    int c;
    while ((c = fgetc(file)) != EOF) {
        int width = c < 10 ? 1 : c < 100 ? 2 : 3;
        if (first) {
            first = 0;
            fprintf(out, "    ");
            pos = 4;
        } else if (pos + width > 74) {
            fprintf(out, ",\n    ");
            pos = 4;
        } else {
            fprintf(out, ", ");
            pos += 2;
        }
        fprintf(out, "%d", c);
        pos += width;
    }
}

int main(int argc, char *argv[]) {
    FILE *conf;
    FILE *inp;
    char line[256];
    char iconname[100][256];
    int nicons = 0, i;

    out = fopen(ICONS_C, "w");
    if (out == NULL) {
        fprintf(stderr, "%s: can't open output file \"%s\"\n", ICON2C, ICONS_C);
        return 1;
    }

    fprintf(out,
        "/* %s\n"
        " * Contains the built-in icons for Free42 (iPhone).\n"
        " * This file is generated by the %s program,\n"
        " * under control of the %s file, which is\n"
        " * a list of icon file names.\n"
        " * The icons are looked for in the current directory\n"
        " * NOTE: this is a generated file; do not edit!\n"
        " */\n\n", ICONS_C, ICON2C, ICON2C_CONF);

    conf = fopen(ICON2C_CONF, "r");
    if (conf == NULL) {
        int err = errno;
        fprintf(stderr, "Can't open \"%s\": %s (%d).\n",
                        ICON2C_CONF, strerror(err), err);
        fclose(out);
        remove(ICONS_C);
        return 1;
    }

    while (1) {
        int len;

        if (fgets(line, 256, conf) == NULL)
            break;
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = 0;
        strcpy(iconname[nicons], line);

        nicons++;
    }
    fclose(conf);


    fprintf(out, "/****************************************/\n");
    fprintf(out, "/* Number of icons defined in this file */\n");
    fprintf(out, "/****************************************/\n\n");

    fprintf(out, "int icon_count = %d;\n\n\n", nicons);


    fprintf(out, "/**************/\n");
    fprintf(out, "/* Icon files */\n");
    fprintf(out, "/**************/\n\n");

    fprintf(out, "const char *icon_name[] = {\n");
    for (i = 0; i < nicons; i++)
        fprintf(out, "    \"%s\"%s\n", iconname[i], i < nicons - 1 ? "," : "");
    fprintf(out, "};\n\n\n");


    fprintf(out, "/***********************/\n");
    fprintf(out, "/* Sizes of icon files */\n");
    fprintf(out, "/***********************/\n\n");
    
    // TODO: If I put 'const' here, the symbol is not exported. Why?
    fprintf(out, "/*const*/ long icon_size[] = {\n");
    for (i = 0; i < nicons; i++) {
        inp = fopen(iconname[i], "rb");
        if (inp == NULL) {
            int err = errno;
            fprintf(stderr, "Can't open \"%s\": %s (%d)\n",
                        iconname[i], strerror(err), err);
            fclose(out);
            remove(ICONS_C);
            return 1;
        }
        fseek(inp, 0, SEEK_END);
        fprintf(out, "    %ld%s\n", ftell(inp), i < nicons - 1 ? "," : "");
        fclose(inp);
    }
    fprintf(out, "};\n\n\n");


    fprintf(out, "/*************/\n");
    fprintf(out, "/* Icon data */\n");
    fprintf(out, "/*************/\n\n");

    for (i = 0; i < nicons; i++) {
        inp = fopen(iconname[i], "rb");
        if (inp == NULL) {
            int err = errno;
            fprintf(stderr, "Can't open \"%s\": %s (%d)\n",
                        iconname[i], strerror(err), err);
            fclose(out);
            remove(ICONS_C);
            return 1;
        }
        fprintf(out, "static const unsigned char icon%d_data[] = {\n", i);
        write_bytes(inp);
        fprintf(out, "\n};\n\n");
        fclose(inp);
    }
    fprintf(out, "/*const*/ unsigned char *icon_data[] = {\n");
    for (i = 0; i < nicons; i++)
        fprintf(out, "    (unsigned char *) icon%d_data%s\n", i, i <
                nicons - 1 ? "," : "");
    fprintf(out, "};\n\n\n");


    fprintf(out, "/***********/\n");
    fprintf(out, "/* The End */\n");
    fprintf(out, "/***********/\n");
    fclose(out);
    return 0;
}
