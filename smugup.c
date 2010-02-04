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
	fprintf(stdout, "smugup - upload photos to smugmug.com\n");
	fprintf(stdout, "Version: " SMUGBATCH_VERSION "\n");
	fprintf(stdout, "Usage:\n");
	fprintf(stdout, "  smugup [options] files\n");
	fprintf(stdout, "options are:\n");
	fprintf(stdout, "  --email email@address\n");
	fprintf(stdout, "  --password password\n");
	fprintf(stdout, "  --album album [ --category category"
				" --subcategory subcategory"
				" --settings quicksettings ]\n");
	fprintf(stdout, "  --quiet\n");
	fprintf(stdout, "  --debug\n");
	fprintf(stdout, "  --help\n");
}

int main(int argc, char *argv[], char *envp[])
{
	static const struct option options[] = {
		{ "debug", 0, NULL, 'd' },
		{ "email", 1, NULL, 'e' },
		{ "password", 1, NULL, 'p' },
		{ "album", 1, NULL, 'a' },
		{ "category", 1, NULL, 'c' },
		{ "subcategory", 1, NULL, 'u' },
		{ "settings", 1, NULL, 's' },
		{ "help", 0, NULL, 'h' },
		{ "quiet", 0, NULL, 'q' },
		{ }
	};
	struct filename *filename;
	struct session *session;
	struct album *album;
	char *album_title = NULL;
	char *category_title = NULL;
	char *subcategory_title = NULL;
	char *quicksettings_name = NULL;
	int retval;
	int option;
	int i;

	session = session_alloc();
	if (!session) {
		fprintf(stderr, "no more memory...\n");
		return -1;
	}

	curl_global_init(CURL_GLOBAL_ALL);
	smug_parse_configfile(session);

	while (1) {
		option = getopt_long_only(argc, argv, "dqe:p:a:c:s:u:h",
					  options, NULL);
		if (option == -1)
			break;
		switch (option) {
		case 'd':
			debug = 1;
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
		case 'c':
			category_title = strdup(optarg);
			dbg("category_title = %s\n", category_title);
			break;
		case 'u':
			subcategory_title = strdup(optarg);
			dbg("subcategory_title = %s\n", subcategory_title);
			break;
		case 's':
			quicksettings_name = strdup(optarg);
			dbg("quicksettings_name = %s\n", quicksettings_name);
			break;
		case 'q':
			session->quiet = 1;
			break;
		case 'h':
			display_help();
			goto exit;
		default:
			display_help();
			goto exit;
		}
	}

	if ((category_title || subcategory_title || quicksettings_name)
			&& !album_title) {
		fprintf(stdout, "(Sub)Category and Settings requires Title.");
		display_help();
		goto exit;
	}

	if (!session->email) {
		fprintf(stdout, "Enter smugmug.com email address: ");
		session->email = get_string_from_stdin();
	}

	if (!session->password) {
		fprintf(stdout, "Enter smugmug.com password: ");
		session->password = get_string_from_stdin();
	}


	/* build up a list of all filenames to be used here */
	for (i = optind; i < argc; ++i) {
		filename = zalloc(sizeof(*filename));
		if (!filename)
			return -ENOMEM;
		filename->filename = strdup(argv[i]);
		filename->basename = my_basename(filename->filename);
		dbg("adding filename '%s'\n", argv[i]);
		list_add_tail(&filename->entry, &session->files_upload);
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

	retval = generate_md5s(&session->files_upload);
	if (retval) {
		fprintf(stderr, "Error calculating md5s\n");
		return -1;
	}
	album = select_album(album_title, category_title, subcategory_title,
			     quicksettings_name, session);
	if (!album)
		return -1;
	retval = upload_files(session, album);
	if (retval) {
		fprintf(stderr, "Error uploading files\n");
		return -1;
	}

	smug_logout(session);

	session_free(session);
exit:
	return 0;
}
