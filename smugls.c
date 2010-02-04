/*
 * Copyright (C) 2008 Greg Kroah-Hartman <greg@kroah.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "list.h"
#include "md5.h"
#include "smugbatch_version.h"
#include "smug.h"


int debug;

static void display_help(void)
{
	fprintf(stdout, "smugls - list files in a smugmug.com album\n");
	fprintf(stdout, "Version: " SMUGBATCH_VERSION "\n");
	fprintf(stdout, "Usage:\n");
	fprintf(stdout, "  smugls [options] files\n");
	fprintf(stdout, "options are:\n");
	fprintf(stdout, "  --email email@address\n");
	fprintf(stdout, "  --password password\n");
	fprintf(stdout, "  --album album\n");
	fprintf(stdout, "  --field-separator\n");
	fprintf(stdout, "  --long\n");
	fprintf(stdout, "  --version\n");
	fprintf(stdout, "  --debug\n");
	fprintf(stdout, "  --help\n");
}

static void display_version(void)
{
	fprintf(stdout, "smugls - version %s\n", SMUGBATCH_VERSION);
}

int main(int argc, char *argv[], char *envp[])
{
	static const struct option options[] = {
		{ "debug", 0, NULL, 'd' },
		{ "email", 1, NULL, 'e' },
		{ "password", 1, NULL, 'p' },
		{ "album", 1, NULL, 'a' },
		{ "field-separator", 1, NULL, 'f' },
		{ "long", 0, NULL, 'l' },
		{ "version", 0, NULL, 'v' },
		{ "help", 0, NULL, 'h' },
		{ }
	};
	struct session *session;
	struct album *album;
	struct filename *filename;
	char *album_title = NULL;
	int longout = 0;
	int retval;
	int option;
	char field_separator = ' ';

	session = session_alloc();
	if (!session) {
		fprintf(stderr, "no more memory...\n");
		return -1;
	}

	curl_global_init(CURL_GLOBAL_ALL);
	smug_parse_configfile(session);

	while (1) {
		option = getopt_long_only(argc, argv, "dle:p:a:f:h",
					  options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'd':
			debug = 1;
			break;
		case 'l':
			longout = 1;
			break;
		case 'e':
			if (session->email)
				free(session->email);
			session->email = strdup(optarg);
			dbg("email = %s\n", session->email);
			break;
		case 'p':
			if (session->password)
				free(session->password);
			session->password = strdup(optarg);
			dbg("password = %s\n", session->password);
			break;
		case 'a':
			album_title = strdup(optarg);
			dbg("album_title = %s\n", album_title);
			break;
		case 'f':
			field_separator = optarg[0];
			dbg("field_separator = %c\n", field_separator);
			break;
		case 'v':
			display_version();
			goto exit;
		case 'h':
			display_help();
			goto exit;
		default:
			display_help();
			goto exit;
		}
	}

	if (!session->email) {
		fprintf(stdout, "Enter smugmug.com email address: ");
		session->email = get_string_from_stdin();
	}

	if (!session->password) {
		fprintf(stdout, "Enter smugmug.com password: ");
		session->password = get_string_from_stdin();
	}

	retval = smug_login(session);
	if (retval) {
		fprintf(stderr, "Can not login\n");
		return -1;
	}

	dbg("1\n");
	retval = smug_get_albums(session);
	if (retval) {
		fprintf(stderr, "Can not read albums\n");
		return -1;
	}

	album = select_album(album_title, NULL, NULL, NULL, session);
	if (!album)
		return -1;

	retval = smug_read_images(session, album);
	if (retval) {
		fprintf(stderr, "Error uploading files\n");
		return -1;
	}

	/* print out the files */
	if (longout)
		fprintf(stdout, "ID        Key   Filename     Category        Subcategory Size    MD5Sum\n");
	else
		fprintf(stdout, "ID        Key   Filename\n");
	list_for_each_entry(filename, &album->files, entry) {
		char *name = filename->filename;
		if (strlen(name) == 0)
			name = "<no name>";
		if (longout)
			fprintf(stdout, "%s%c%s%c%s%c%s%c%s%c%s%c%s\n",
				filename->id, field_separator,
				filename->key, field_separator,
				name, field_separator,
				album->category, field_separator,
				album->subcategory, field_separator,
				filename->size, field_separator,
				filename->md5_string);
		else
			fprintf(stdout, "%s%c%s%c%s\n",
				filename->id, field_separator,
				filename->key, field_separator,
				name);
	}

	free(album_title);
	smug_logout(session);

	session_free(session);
exit:
	return 0;
}
