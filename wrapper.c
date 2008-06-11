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

#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct cgroup *cgroup_new_cgroup(const char *name, uid_t tasks_uid,
		gid_t tasks_gid, uid_t control_uid, gid_t control_gid)
{
	struct cgroup *cgroup = (struct cgroup *)
					malloc(sizeof(struct cgroup));

	if (!cgroup)
		return NULL;

	strncpy(cgroup->name, name, sizeof(cgroup->name));
	cgroup->tasks_uid = tasks_uid;
	cgroup->tasks_gid = tasks_gid;
	cgroup->control_uid = control_uid;
	cgroup->control_gid = control_gid;
	cgroup->index = 0;

	return cgroup;
}

struct cgroup_controller *cgroup_add_controller(struct cgroup *cgroup,
							const char *name)
{
	int i;
	struct cgroup_controller *controller;

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

	controller = (struct cgroup_controller *)
				malloc(sizeof(struct cgroup_controller));

	if (!controller)
		return NULL;

	strncpy(controller->name, name, sizeof(controller->name));
	controller->index = 0;

	cgroup->controller[cgroup->index] = controller;
	cgroup->index++;

	return controller;
}

void cgroup_free(struct cgroup **cgroup)
{
	int i, j;
	struct cgroup *cg = *cgroup;

	/*
	 * Passing NULL pointers is OK. We just return.
	 */
	if (!cg)
		return;

	for (i = 0; i < cg->index; i++) {
		for (j = 0; j < cg->controller[i]->index; j++)
			free(cg->controller[i]->values[j]);
		free(cg->controller[i]);
	}

	free(cg);
	*cgroup = NULL;
}

int cgroup_add_value_string(struct cgroup_controller *controller,
					const char *name, const char *value)
{
	int i;
	struct control_value *cntl_value = (struct control_value *)
					malloc(sizeof(struct control_value));

	if (!cntl_value)
		return ECGCONTROLLERCREATEFAILED;

	if (controller->index >= CG_VALUE_MAX)
		return ECGMAXVALUESEXCEEDED;

	for (i = 0; i < controller->index && i < CG_VALUE_MAX; i++) {
		if (strncmp(controller->values[controller->index]->name, name,
		sizeof(controller->values[controller->index]->name)) == 0)
			return ECGVALUEEXISTS;
	}


	strncpy(cntl_value->name, name, sizeof(cntl_value->name));
	strncpy(cntl_value->value, value, sizeof(cntl_value->value));
	controller->values[controller->index] = cntl_value;
	controller->index++;

	return 0;
}

int cgroup_add_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t value)
{
	int i;
	unsigned ret;
	struct control_value *cntl_value = (struct control_value *)
					malloc(sizeof(struct control_value));

	if (!cntl_value)
		return ECGCONTROLLERCREATEFAILED;


	if (controller->index >= CG_VALUE_MAX)
		return ECGMAXVALUESEXCEEDED;

	for (i = 0; i < controller->index && i < CG_VALUE_MAX; i++) {
		if (strncmp(controller->values[controller->index]->name, name,
		   sizeof(controller->values[controller->index]->name)) == 0)
			return ECGVALUEEXISTS;
	}

	strncpy(cntl_value->name, name,
			sizeof(cntl_value->name));
	ret = snprintf(cntl_value->value,
	  sizeof(cntl_value->value), "%lld", value);

	if (ret >= sizeof(cntl_value->value))
		return ECGINVAL;

	controller->values[controller->index] = cntl_value;
	controller->index++;

	return 0;

}

int cgroup_add_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t value)
{
	int i;
	unsigned ret;
	struct control_value *cntl_value = (struct control_value *)
					malloc(sizeof(struct control_value));

	if (!cntl_value)
		return ECGCONTROLLERCREATEFAILED;


	if (controller->index >= CG_VALUE_MAX)
		return ECGMAXVALUESEXCEEDED;

	for (i = 0; i < controller->index && i < CG_VALUE_MAX; i++) {
		if (strncmp(controller->values[controller->index]->name, name,
		   sizeof(controller->values[controller->index]->name)) == 0)
			return ECGVALUEEXISTS;
	}

	strncpy(cntl_value->name, name,	sizeof(cntl_value->name));
	ret = snprintf(cntl_value->value, sizeof(cntl_value->value), "%llu", value);

	if (ret >= sizeof(cntl_value->value))
		return ECGINVAL;

	controller->values[controller->index] = cntl_value;
	controller->index++;

	return 0;

}

int cgroup_add_value_bool(struct cgroup_controller *controller,
						const char *name, bool value)
{
	int i;
	unsigned ret;
	struct control_value *cntl_value = (struct control_value *)
					malloc(sizeof(struct control_value));

	if (!cntl_value)
		return ECGCONTROLLERCREATEFAILED;


	if (controller->index >= CG_VALUE_MAX)
		return ECGMAXVALUESEXCEEDED;

	for (i = 0; i < controller->index && i < CG_VALUE_MAX; i++) {
		if (strncmp(controller->values[controller->index]->name, name,
		  sizeof(controller->values[controller->index]->name)) == 0)
			return ECGVALUEEXISTS;
	}

	strncpy(cntl_value->name, name, sizeof(cntl_value->name));

	if (value)
		ret = snprintf(cntl_value->value, sizeof(cntl_value->value), "1");
	else
		ret = snprintf(cntl_value->value, sizeof(cntl_value->value), "0");

	if (ret >= sizeof(cntl_value->value))
		return ECGINVAL;

	controller->values[controller->index] = cntl_value;
	controller->index++;

	return 0;
}
