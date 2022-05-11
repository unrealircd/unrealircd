/** Oper classes code.
 * (C) Copyright 2015-present tmcarthur and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

typedef struct OperClassPathNode OperClassPathNode;
typedef struct OperClassCallbackNode OperClassCallbackNode;

struct OperClassPathNode
{
	OperClassPathNode *prev,*next;
	OperClassPathNode *children;
	char *identifier;
	OperClassCallbackNode *callbacks;
};

struct OperClassCallbackNode
{
	OperClassCallbackNode *prev, *next;
	OperClassPathNode *parent;
	OperClassEntryEvalCallback callback;
};

struct OperClassValidator
{
	Module *owner;
	OperClassCallbackNode *node;
};

OperClassACLPath *OperClass_parsePath(const char *path);
void OperClass_freePath(OperClassACLPath *path);
OperClassPathNode *OperClass_findPathNodeForIdentifier(char *identifier, OperClassPathNode *head);

OperClassPathNode *rootEvalNode = NULL;

OperClassValidator *OperClassAddValidator(Module *module, char *pathStr, OperClassEntryEvalCallback callback)
{
	OperClassPathNode *node,*nextNode;
	OperClassCallbackNode *callbackNode;
	OperClassValidator *validator; 
	OperClassACLPath *path = OperClass_parsePath(pathStr);

	if (!rootEvalNode)
	{
		rootEvalNode = safe_alloc(sizeof(OperClassPathNode));
	}

	node = rootEvalNode;

	while (path)
	{
		nextNode = OperClass_findPathNodeForIdentifier(path->identifier,node->children);
		if (!nextNode)
		{
			nextNode = safe_alloc(sizeof(OperClassPathNode));
			safe_strdup(nextNode->identifier, path->identifier);
			AddListItem(nextNode,node->children);
		}
		node = nextNode;
		path = path->next;
	}

	callbackNode = safe_alloc(sizeof(OperClassCallbackNode));
	callbackNode->callback = callback;
	callbackNode->parent = node;	
	AddListItem(callbackNode,node->callbacks);

	validator = safe_alloc(sizeof(OperClassValidator));
	validator->node = callbackNode;	
	validator->owner = module;

	if (module)
	{
		ModuleObject *mobj = safe_alloc(sizeof(ModuleObject));
		mobj->object.validator = validator;
		mobj->type = MOBJ_VALIDATOR;
		AddListItem(mobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	OperClass_freePath(path);

	return validator;
}

void OperClassValidatorDel(OperClassValidator *validator)
{
	if (validator->owner)
	{
		ModuleObject *mdobj;
		for (mdobj = validator->owner->objects; mdobj; mdobj = mdobj->next)
		{
			if ((mdobj->type == MOBJ_VALIDATOR) && (mdobj->object.validator == validator))
			{
				DelListItem(mdobj, validator->owner->objects);
				safe_free(mdobj);
				break;
			}
		}
		validator->owner = NULL;
	}
	
	/* Technically, the below leaks memory if you don't re-register
	 * another validator at same path, but it is cheaper than walking
	 * back up and doing cleanup in practice, since this tree is very small
	 */
	DelListItem(validator->node,validator->node->parent->callbacks);
	safe_free(validator->node);
	safe_free(validator);	
}

OperClassACLPath *OperClass_parsePath(const char *path)
{
	char *pathCopy = raw_strdup(path);
	OperClassACLPath *pathHead = NULL;
	OperClassACLPath *tmpPath;
	char *str = strtok(pathCopy,":");
	while (str)
	{
		tmpPath = safe_alloc(sizeof(OperClassACLPath));
		safe_strdup(tmpPath->identifier, str);
		AddListItem(tmpPath,pathHead);
		str = strtok(NULL,":");
	}

	while (pathHead->next)
	{
		tmpPath = pathHead->next;
		pathHead->next = pathHead->prev;
		pathHead->prev = tmpPath;
		pathHead = tmpPath;
	}
	pathHead->next = pathHead->prev;
	pathHead->prev = NULL;	

	safe_free(pathCopy);
	return pathHead;
}

void OperClass_freePath(OperClassACLPath *path)
{
	OperClassACLPath *next;
	while (path)
	{
		next = path->next;
		safe_free(path->identifier);
		safe_free(path);
		path = next;
	}	
}

OperClassACL *OperClass_FindACL(OperClassACL *acl, char *name)
{
	for (;acl;acl = acl->next)
	{
		if (!strcmp(acl->name,name))
		{ 
			return acl;
		}
	}
	return NULL;
}

OperClassPathNode *OperClass_findPathNodeForIdentifier(char *identifier, OperClassPathNode *head)
{
	for (; head; head = head->next)
	{
		if (!strcmp(head->identifier,identifier))
		{
			return head;
		}
	}
	return NULL;
}

unsigned char OperClass_evaluateACLEntry(OperClassACLEntry *entry, OperClassACLPath *path, OperClassCheckParams *params)
{
	OperClassPathNode *node = rootEvalNode;	
	OperClassCallbackNode *callbackNode = NULL;
	unsigned char eval = 0;

	/* If no variables, always match */
	if (!entry->variables)
	{
		return 1;
	}

	/* Go as deep as possible */
	while (path->next && node)
	{
		node = OperClass_findPathNodeForIdentifier(path->identifier,node);	
		/* If we can't find a node we need, and we have vars, no match */
		if (!node)
		{
			return 0;
		}
		node = node->children;
		path = path->next;
	}

	/* If no evals for full path, no match */
	if (path->next)
	{
		return 0;
	}


	/* We have a valid node, execute all callback nodes */
	for (callbackNode = node->callbacks; callbackNode; callbackNode = callbackNode->next)
	{
		eval = callbackNode->callback(entry->variables,params);
	}

	return eval;	
}

OperPermission ValidatePermissionsForPathEx(OperClassACL *acl, OperClassACLPath *path, OperClassCheckParams *params)
{
	/** Evaluate into ACL struct as deep as possible **/
	OperClassACLPath *basePath = path;
	OperClassACL *tmp;
	OperClassACLEntry *entry;
	unsigned char allow = 0;
	unsigned char deny = 0;
	unsigned char aclNotFound = 0;

	path = path->next; /* Avoid first level since we have resolved it */
	while (path && acl->acls)
	{
		tmp = OperClass_FindACL(acl->acls,path->identifier);
		if (!tmp)
		{
			aclNotFound = 1;
			break;
		}
		path = path->next;
		acl = tmp;
	}
	/** If node does not exist, but most specific one has other ACLs, deny **/
	if (acl->acls && aclNotFound)
	{
		return OPER_DENY;
	}

	/** If node exists for this but has no ACL entries, allow **/
	if (!acl->entries)
	{
		return OPER_ALLOW;
	}
	/** Process entries **/
	for (entry = acl->entries; entry; entry = entry->next)
	{
		unsigned char result;
		/* Short circuit if we already have valid block */
		if (entry->type == OPERCLASSENTRY_ALLOW && allow)
			continue;
		if (entry->type == OPERCLASSENTRY_DENY && deny)
			continue;

		result = OperClass_evaluateACLEntry(entry,basePath,params);
		if (entry->type == OPERCLASSENTRY_ALLOW)
		{
			allow = result;
		}
		else
		{
			deny = result;
		}
	}

	/** We only permit if an allow matched AND no deny matched **/
	if (allow && !deny)
	{
		return OPER_ALLOW;
	}

	return OPER_DENY;
}

OperPermission ValidatePermissionsForPath(const char *path, Client *client, Client *victim, Channel *channel, const void *extra)
{
	ConfigItem_oper *ce_oper;
	const char *operclass;
	ConfigItem_operclass *ce_operClass;
	OperClass *oc = NULL;
	OperClassACLPath *operPath;

	if (!client)
		return OPER_DENY;

	/* Trust Servers, U-Lines and remote opers */
	if (IsServer(client) || IsULine(client) || (IsOper(client) && !MyUser(client)))
		return OPER_ALLOW;

	if (!IsOper(client))
		return OPER_DENY;

	ce_oper = find_oper(client->user->operlogin);
	if (!ce_oper)
	{
		operclass = moddata_client_get(client, "operclass");
		if (!operclass)
			return OPER_DENY;
	} else
	{
		operclass = ce_oper->operclass;
	}

	ce_operClass = find_operclass(operclass);
	if (!ce_operClass)
		return OPER_DENY;

	oc = ce_operClass->classStruct;
	operPath = OperClass_parsePath(path);
	while (oc && operPath)
	{
		OperClassACL *acl = OperClass_FindACL(oc->acls,operPath->identifier);
		if (acl)
		{
			OperPermission perm;
			OperClassCheckParams *params = safe_alloc(sizeof(OperClassCheckParams));
			params->client = client;
			params->victim = victim;
			params->channel = channel;
			params->extra = extra;
			
			perm = ValidatePermissionsForPathEx(acl, operPath, params);
			OperClass_freePath(operPath);
			safe_free(params);
			return perm;
		}
		if (!oc->ISA)
		{
			break;
		}
		ce_operClass = find_operclass(oc->ISA);
		if (ce_operClass)
		{
			oc = ce_operClass->classStruct;
		} else {
			break; /* parent not found */
		}
	}
	OperClass_freePath(operPath);
	return OPER_DENY;
}
