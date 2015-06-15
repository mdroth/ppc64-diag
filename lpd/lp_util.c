/**
 * @file	lp_util.c
 * @brief	Light Path utility
 *
 * Copyright (C) 2012 IBM Corporation
 * See 'COPYRIGHT' for License of this code.
 *
 * @author	Vasant Hegde <hegdevasant@linux.vnet.ibm.com>
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include "lp_diag.h"
#include "utils.h"

#define LSVPD_PATH	"/usr/sbin/lsvpd"

/**
 * fgets_nonl - Read a line and strip the newline.
 */
char *
fgets_nonl(char *buf, int size, FILE *s)
{
	char *nl;

	if (!fgets(buf, size, s))
		return NULL;
	nl = strchr(buf, '\n');
	if (nl == NULL)
		return NULL;
	*nl = '\0';
	return buf;
}

/**
 * skip_space -
 */
static char *
skip_space(const char *pos)
{
	pos = strchr(pos, ' ');
	if (!pos)
		return NULL;
	while (*pos == ' ')
		pos++;
	return (char *)pos;
}

/**
 * read_vpd_from_lsvpd - Retrieve vpd data
 *
 * Returns :
 *	0 on success, !0 on failure
 */
int
read_vpd_from_lsvpd(struct dev_vpd *vpd, const char *device)
{
	char	buf[128];
	char	*pos;
	char	*args[] = {LSVPD_PATH, "-l", (char *const)device, NULL};
	pid_t	cpid;
	int	rc;
	FILE	*fp;

	/* use lsvpd to find the device vpd */
	fp = spopen(args, &cpid);
	if (fp == NULL) {
		log_msg("Unable to retrieve the vpd for \"%s\". "
			"Ensure that lsvpd package is installed.", device);
		return -1;
	}

	while (fgets_nonl(buf, 128, fp) != NULL) {
		if ((pos = strstr(buf, "*TM")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->mtm, pos, VPD_LENGTH - 1);
			vpd->mtm[VPD_LENGTH - 1] = '\0';
		} else if ((pos = strstr(buf, "*YL")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->location, pos, LOCATION_LENGTH - 1);
			vpd->location[LOCATION_LENGTH - 1] = '\0';
		} else if ((pos = strstr(buf, "*DS")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->ds, pos, VPD_LENGTH - 1);
			vpd->ds[VPD_LENGTH - 1] = '\0';
		} else if ((pos = strstr(buf, "*SN")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->sn, pos, VPD_LENGTH - 1);
			vpd->sn[VPD_LENGTH - 1] = '\0';
		} else if ((pos = strstr(buf, "PN")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->pn, pos, VPD_LENGTH - 1);
			vpd->pn[VPD_LENGTH - 1] = '\0';
		} else if ((pos = strstr(buf, "*FN")) != NULL) {
			if (!(pos = skip_space(pos)))
				continue;
			strncpy(vpd->fru, pos, VPD_LENGTH - 1);
			vpd->fru[VPD_LENGTH - 1] = '\0';
		}
	}

	rc = spclose(fp, cpid);
	if (rc) {
		log_msg("spclose() failed [rc=%d]\n", rc);
		return -1;
	}

	return 0;
}

/**
 * free_device_vpd - free vpd structure
 */
void
free_device_vpd(struct dev_vpd *vpd)
{
	struct	dev_vpd *tmp;

	while (vpd) {
		tmp = vpd;
		vpd = vpd->next;
		free(tmp);
	}
}

/**
 * read_device_vpd -
 *
 * @path	/sys path
 *
 * Returns :
 *	vpd structure on success / NULL on failure
 */
struct dev_vpd *
read_device_vpd(const char *path)
{
	DIR	*dir;
	struct	dirent *dirent;
	struct	dev_vpd *vpd = NULL;
	struct	dev_vpd *curr = NULL;

	dir = opendir(path);
	if (!dir) {
		if (errno != ENOENT)
			log_msg("Unable to open directory : %s", path);
		return NULL;
	}

	while ((dirent = readdir(dir)) != NULL) {
		if (!strcmp(dirent->d_name, ".") ||
		    !strcmp(dirent->d_name, ".."))
			continue;

		if (!curr) {
			curr = calloc(1, sizeof(struct dev_vpd));
			vpd = curr;
		} else {
			curr->next = calloc(1, sizeof(struct dev_vpd));
			curr = curr->next;
		}
		if (!curr) {
			log_msg("Out of memory");
			free_device_vpd(vpd);
			closedir(dir);
			return NULL;
		}

		/* device name */
		strncpy(curr->dev, dirent->d_name, DEV_LENGTH - 1);
		curr->dev[DEV_LENGTH - 1] = '\0';

		if (read_vpd_from_lsvpd(curr, dirent->d_name)) {
			free_device_vpd(vpd);
			closedir(dir);
			return NULL;
		}
	}
	closedir(dir);
	return vpd;
}
