/*
 * Copyright IBM Corporation. 2008
 *
 * Author:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void init_cgroup(struct cgroup *cgroup)
{
	cgroup->task_fperm = cgroup->control_fperm = cgroup->control_dperm = NO_PERMS;
	cgroup->control_gid = cgroup->control_uid = cgroup->tasks_gid =
			cgroup->tasks_uid = NO_UID_GID;
}

void init_cgroup_table(struct cgroup *cgroups, size_t count)
{
	size_t i;

	for (i = 0; i < count; ++i)
		init_cgroup(&cgroups[i]);
}

struct cgroup *cgroup_new_cgroup(const char *name)
{
	struct cgroup *cgroup = calloc(1, sizeof(struct cgroup));
	if (!cgroup)
		return NULL;

	init_cgroup(cgroup);
	strncpy(cgroup->name, name, sizeof(cgroup->name));

	return cgroup;
}

struct cgroup_controller *cgroup_add_controller(struct cgroup *cgroup,
							const char *name)
{
	int i;
	struct cgroup_controller *controller;

	if (!cgroup)
		return NULL;

	/*
	 * Still not sure how to handle the failure here.
	 */
	if (cgroup->index >= CG_CONTROLLER_MAX)
		return NULL;

	/*
	 * Still not sure how to handle the failure here.
	 */
	for (i = 0; i < cgroup->index; i++) {
		if (strncmp(name, cgroup->controller[i]->name,
				sizeof(cgroup->controller[i]->name)) == 0)
			return NULL;
	}

	controller = calloc(1, sizeof(struct cgroup_controller));

	if (!controller)
		return NULL;

	strncpy(controller->name, name, sizeof(controller->name));
	controller->cgroup = cgroup;
	controller->index = 0;

	cgroup->controller[cgroup->index] = controller;
	cgroup->index++;

	return controller;
}

void cgroup_free_controllers(struct cgroup *cgroup)
{
	int i, j;

	if (!cgroup)
		return;

	for (i = 0; i < cgroup->index; i++) {
		for (j = 0; j < cgroup->controller[i]->index; j++)
			free(cgroup->controller[i]->values[j]);
		cgroup->controller[i]->index = 0;
		free(cgroup->controller[i]);
	}
	cgroup->index = 0;
}

void cgroup_free(struct cgroup **cgroup)
{
	struct cgroup *cg = *cgroup;

	/*
	 * Passing NULL pointers is OK. We just return.
	 */
	if (!cg)
		return;

	cgroup_free_controllers(cg);
	free(cg);
	*cgroup = NULL;
}

int cgroup_add_value_string(struct cgroup_controller *controller,
					const char *name, const char *value)
{
	int i;
	struct control_value *cntl_value;

	if (!controller)
		return ECGINVAL;

	if (controller->index >= CG_VALUE_MAX)
		return ECGMAXVALUESEXCEEDED;

	for (i = 0; i < controller->index && i < CG_VALUE_MAX; i++) {
		if (!strcmp(controller->values[i]->name, name))
			return ECGVALUEEXISTS;
	}

	cntl_value = calloc(1, sizeof(struct control_value));

	if (!cntl_value)
		return ECGCONTROLLERCREATEFAILED;

	strncpy(cntl_value->name, name, sizeof(cntl_value->name));
	strncpy(cntl_value->value, value, sizeof(cntl_value->value));
	cntl_value->dirty = true;
	controller->values[controller->index] = cntl_value;
	controller->index++;

	return 0;
}

int cgroup_add_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t value)
{
	int ret;
	char *val;

	ret = asprintf(&val, "%"PRId64, value);
	if (ret < 0) {
		last_errno = errno;
		return ECGOTHER;
	}

	ret = cgroup_add_value_string(controller, name, val);
	free(val);

	return ret;
}

int cgroup_add_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t value)
{
	int ret;
	char *val;

	ret = asprintf(&val, "%" PRIu64, value);
	if (ret < 0) {
		last_errno = errno;
		return ECGOTHER;
	}

	ret = cgroup_add_value_string(controller, name, val);
	free(val);

	return ret;
}

int cgroup_add_value_bool(struct cgroup_controller *controller,
						const char *name, bool value)
{
	int ret;
	char *val;

	if (value)
		val = strdup("1");
	else
		val = strdup("0");
	if (!val) {
		last_errno = errno;
		return ECGOTHER;
	}

	ret = cgroup_add_value_string(controller, name, val);
	free(val);

	return ret;
}

int cgroup_compare_controllers(struct cgroup_controller *cgca,
					struct cgroup_controller *cgcb)
{
	int i;

	if (!cgca || !cgcb)
		return ECGINVAL;

	if (strcmp(cgca->name, cgcb->name))
		return ECGCONTROLLERNOTEQUAL;

	if (cgca->index != cgcb->index)
		return ECGCONTROLLERNOTEQUAL;

	for (i = 0; i < cgca->index; i++) {
		struct control_value *cva = cgca->values[i];
		struct control_value *cvb = cgcb->values[i];

		if (strcmp(cva->name, cvb->name))
			return ECGCONTROLLERNOTEQUAL;

		if (strcmp(cva->value, cvb->value))
			return ECGCONTROLLERNOTEQUAL;
	}
	return 0;
}

int cgroup_compare_cgroup(struct cgroup *cgroup_a, struct cgroup *cgroup_b)
{
	int i;

	if (!cgroup_a || !cgroup_b)
		return ECGINVAL;

	if (strcmp(cgroup_a->name, cgroup_b->name))
		return ECGROUPNOTEQUAL;

	if (cgroup_a->tasks_uid != cgroup_b->tasks_uid)
		return ECGROUPNOTEQUAL;

	if (cgroup_a->tasks_gid != cgroup_b->tasks_gid)
		return ECGROUPNOTEQUAL;

	if (cgroup_a->control_uid != cgroup_b->control_uid)
		return ECGROUPNOTEQUAL;

	if (cgroup_a->control_gid != cgroup_b->control_gid)
		return ECGROUPNOTEQUAL;

	if (cgroup_a->index != cgroup_b->index)
		return ECGROUPNOTEQUAL;

	for (i = 0; i < cgroup_a->index; i++) {
		struct cgroup_controller *cgca = cgroup_a->controller[i];
		struct cgroup_controller *cgcb = cgroup_b->controller[i];

		if (cgroup_compare_controllers(cgca, cgcb))
			return ECGCONTROLLERNOTEQUAL;
	}
	return 0;
}

int cgroup_set_uid_gid(struct cgroup *cgroup, uid_t tasks_uid, gid_t tasks_gid,
					uid_t control_uid, gid_t control_gid)
{
	if (!cgroup)
		return ECGINVAL;

	cgroup->tasks_uid = tasks_uid;
	cgroup->tasks_gid = tasks_gid;
	cgroup->control_uid = control_uid;
	cgroup->control_gid = control_gid;

	return 0;
}

int cgroup_get_uid_gid(struct cgroup *cgroup, uid_t *tasks_uid,
		gid_t *tasks_gid, uid_t *control_uid, gid_t *control_gid)
{
	if (!cgroup)
		return ECGINVAL;

	*tasks_uid = cgroup->tasks_uid;
	*tasks_gid = cgroup->tasks_gid;
	*control_uid = cgroup->control_uid;
	*control_gid = cgroup->control_gid;

	return 0;
}

struct cgroup_controller *cgroup_get_controller(struct cgroup *cgroup,
							const char *name)
{
	int i;
	struct cgroup_controller *cgc;

	if (!cgroup)
		return NULL;

	for (i = 0; i < cgroup->index; i++) {
		cgc = cgroup->controller[i];

		if (!strcmp(cgc->name, name))
			return cgc;
	}

	return NULL;
}

int cgroup_get_value_string(struct cgroup_controller *controller,
					const char *name, char **value)
{
	int i;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {
			*value = strdup(val->value);

			if (!*value)
				return ECGOTHER;

			return 0;
		}
	}

	return ECGROUPVALUENOTEXIST;

}

int cgroup_set_value_string(struct cgroup_controller *controller,
					const char *name, const char *value)
{
	int i;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];
		if (!strcmp(val->name, name)) {
			strncpy(val->value, value, CG_VALUE_MAX);
			val->dirty = true;
			return 0;
		}
	}

	return cgroup_add_value_string(controller, name, value);
}

int cgroup_get_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t *value)
{
	int i;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {

			if (sscanf(val->value, "%" SCNd64, value) != 1)
				return ECGINVAL;

			return 0;
		}
	}

	return ECGROUPVALUENOTEXIST;
}

int cgroup_set_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t value)
{
	int i;
	int ret;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {
			ret = snprintf(val->value,
				sizeof(val->value), "%" PRId64, value);

			if (ret >= sizeof(val->value) || ret < 0)
				return ECGINVAL;

			val->dirty = true;
			return 0;
		}
	}

	return cgroup_add_value_int64(controller, name, value);
}

int cgroup_get_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t *value)
{
	int i;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];
		if (!strcmp(val->name, name)) {

			if (sscanf(val->value, "%" SCNu64, value) != 1)
				return ECGINVAL;

			return 0;
		}
	}

	return ECGROUPVALUENOTEXIST;
}

int cgroup_set_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t value)
{
	int i;
	int ret;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {
			ret = snprintf(val->value,
				sizeof(val->value), "%" PRIu64, value);

			if (ret >= sizeof(val->value) || ret < 0)
				return ECGINVAL;

			val->dirty = true;
			return 0;
		}
	}

	return cgroup_add_value_uint64(controller, name, value);
}

int cgroup_get_value_bool(struct cgroup_controller *controller,
						const char *name, bool *value)
{
	int i;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {
			int cgc_val;

			if (sscanf(val->value, "%d", &cgc_val) != 1)
				return ECGINVAL;

			if (cgc_val)
				*value = true;
			else
				*value = false;

			return 0;
		}
	}
	return ECGROUPVALUENOTEXIST;
}

int cgroup_set_value_bool(struct cgroup_controller *controller,
						const char *name, bool value)
{
	int i;
	int ret;

	if (!controller)
		return ECGINVAL;

	for (i = 0; i < controller->index; i++) {
		struct control_value *val = controller->values[i];

		if (!strcmp(val->name, name)) {
			if (value) {
				ret = snprintf(val->value,
						sizeof(val->value), "1");
			} else {
				ret = snprintf(val->value,
						sizeof(val->value), "0");
			}

			if (ret >= sizeof(val->value) || ret < 0)
				return ECGINVAL;

			val->dirty = true;
			return 0;

		}
	}

	return cgroup_add_value_bool(controller, name, value);
}

struct cgroup *create_cgroup_from_name_value_pairs(const char *name,
		struct control_value *name_value, int nv_number)
{
	struct cgroup *src_cgroup;
	struct cgroup_controller *cgc;
	char con[FILENAME_MAX];

	int ret;
	int i;

	/* create source cgroup */
	src_cgroup = cgroup_new_cgroup(name);
	if (!src_cgroup) {
		fprintf(stderr, "can't create cgroup: %s\n",
			cgroup_strerror(ECGFAIL));
		goto scgroup_err;
	}

	/* add pairs name-value to
	   relevant controllers of this cgroup */
	for (i = 0; i < nv_number; i++) {

		if ((strchr(name_value[i].name, '.')) == NULL) {
			fprintf(stderr, "wrong -r  parameter (%s=%s)\n",
				name_value[i].name, name_value[i].value);
			goto scgroup_err;
		}

		strncpy(con, name_value[i].name, FILENAME_MAX);
		strtok(con, ".");

		/* find out whether we have to add the controller or
		   cgroup already contains it */
		cgc = cgroup_get_controller(src_cgroup, con);
		if (!cgc) {
			/* add relevant controller */
			cgc = cgroup_add_controller(src_cgroup, con);
			if (!cgc) {
				fprintf(stderr, "controller %s can't be add\n",
						con);
				goto scgroup_err;
			}
		}

		/* add name-value pair to this controller */
		ret = cgroup_add_value_string(cgc,
			name_value[i].name, name_value[i].value);
		if (ret) {
			fprintf(stderr, "name-value pair %s=%s can't be set\n",
				name_value[i].name, name_value[i].value);
			goto scgroup_err;
		}
	}

	return src_cgroup;
scgroup_err:
	cgroup_free(&src_cgroup);
	return NULL;
}

int cgroup_get_value_name_count(struct cgroup_controller *controller)
{
	if (!controller)
		return -1;

	return controller->index;
}


char *cgroup_get_value_name(struct cgroup_controller *controller, int index)
{

	if (!controller)
		return NULL;

	if (index < controller->index)
		return (controller->values[index])->name;
	else
		return NULL;
}

char *cgroup_get_cgroup_name(struct cgroup *cgroup)
{
	if (!cgroup)
		return NULL;

	return cgroup->name;
}
