/**
OpenIO SDS oio-grep, a demonstration tool for the C client API.
Copyright (C) 2015 OpenIO, original work as part of OpenIO Software Defined Storage

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <grid_client.h>

static char *ns;
static char *pattern;

struct matcher_s {
	const char *container, *content;
	char line[8192];
	int index;
};

static void
_reset_matcher (struct matcher_s *matcher)
{
	memset (matcher->line, 0, sizeof(matcher->line));
	matcher->index = 0;
}

static void
_check_and_dump (struct matcher_s *matcher)
{
	if (strstr (matcher->line, pattern))
		fprintf(stdout, "%s:%s:%s\n", matcher->container, matcher->content, matcher->line);
}

/* XXX
 * In this example we perform a 'custom' download. This means we manage the
 * data. This function is responsible to return the number of bytes actually
 * managed. Return something negative to notify the API there was an error. */
static ssize_t
_write_matches (struct matcher_s *matcher, const char *b, const size_t bSize)
{
	if (!b || !bSize)  { /* final tail call */
		_check_and_dump (matcher);
		_reset_matcher (matcher);
		return 0;
	}

	for (size_t i=0; i<bSize ;i++) {
		if (b[i] == '\n') {
			_check_and_dump (matcher);
			_reset_matcher (matcher);
		} else {
			if (i >= sizeof(matcher->line)-1) {
				_check_and_dump (matcher);
				_reset_matcher (matcher);
			}
			if (b[i] != '\r')
				matcher->line[ matcher->index++ ] = b[i];
		}
	}

	return (ssize_t)bSize;
}

static void
_download (gs_grid_storage_t *gs, const char *container, const char *content)
{
	struct matcher_s *matcher = malloc(sizeof(struct matcher_s));
	_reset_matcher (matcher);
	matcher->container = container;
	matcher->content = content;

	struct gs_download_info_s context;
	context.offset = 0;
	context.size = 0;
	context.user_data = matcher;
	context.writer = (gs_output_f) _write_matches;

	/* XXX
	 * Once the client has been initiated, there is a simple API call to
	 * perform a download. This will manage the container's location, the
	 * chunks locations, etc. By default, no autocreation is asked. */
	gs_error_t *err = NULL;
	gs_status_t gsrc = hc_dl_content_custom (gs, container, content, &context, &err);
	if (gsrc != GS_OK) {
		fflush(stdout);
		fprintf(stderr, "OIO download error on [%s]/[%s] : (%d) %s\n",
				container, content, err->code, err->msg);
		gs_error_free (err);
		err = NULL;
	} else {
		fflush(stdout);
		fprintf(stderr, "OIO download done on [%s]/[%s]\n", container, content);
	}

	free (matcher);
}

int
main (int argc, char **args)
{
	if (argc < 3) {
		fprintf (stderr, "Usage: %s NAMESPACE PATTERN [CONTAINER/CONTENT]...\n", args[0]);
		return 1;
	}

	ns = args[1];
	pattern = args[2];

	int rc = 0;

	/* XXX
	 * The only information necessary to initiate a new SDS client is the name 
	 * of the namespace. Internally, it will be resolved into the conscience's
	 * ip/port couple. */
	gs_error_t *err = NULL;
	gs_grid_storage_t *gs = gs_grid_storage_init (ns, &err);
	if (!gs) {
		fprintf(stderr, "OIO init error : (%d) %s\n", err->code, err->msg);
		gs_error_free (err);
		err = NULL;
		rc = 2;
	} else {
		for (int i=3; i<argc ;i++) {
			char *container, *separator, *content;
			container = strdup(args[i]);
			separator = strchr(container, '/');
			if (!separator) {
				fprintf (stderr, "LOCAL format error for [%s] : expecting CONTAINER/CONTENT path\n", container);
			} else {
				*separator = 0;
				content = separator + 1;
				_download (gs, container, content);
			}
			free(container);
		}

		/* XXX
		 * This will free all the data internally linked to the gs_grid_storage_t
		 * structure. */
		gs_grid_storage_free (gs);
		gs = NULL;
	}

	return rc;
}

