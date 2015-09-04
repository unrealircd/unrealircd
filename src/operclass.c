#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

typedef struct _operClass_PathNode OperClass_PathNode;
typedef struct _operClass_CallbackNode OperClass_CallbackNode;

struct _operClass_PathNode
{
	OperClass_PathNode *prev,*next;
	OperClass_PathNode *children;
	char* identifier;
	OperClass_CallbackNode* callbacks;
};

struct _operClass_CallbackNode
{
	OperClass_CallbackNode *prev, *next;
	OperClass_PathNode *parent;
	OperClassEntryEvalCallback callback;
};

struct _operClass_Validator
{
	Module* owner;
	OperClass_CallbackNode* node;
};

OperClassACLPath* OperClass_parsePath(char* path);
void OperClass_freePath(OperClassACLPath* path);
OperClass_PathNode* OperClass_findPathNodeForIdentifier(char* identifier, OperClass_PathNode *head);

OperClass_PathNode* rootEvalNode = NULL;

OperClassValidator* OperClassAddValidator(Module *module, char* pathStr, OperClassEntryEvalCallback callback)
{
	OperClass_PathNode *node,*nextNode;
	OperClass_CallbackNode *callbackNode;
	OperClassValidator *validator; 
	OperClassACLPath* path = OperClass_parsePath(pathStr);

	if (!rootEvalNode)
	{
		rootEvalNode = MyMallocEx(sizeof(OperClass_PathNode));
	}

	node = rootEvalNode;

	while (path)
        {
                nextNode = OperClass_findPathNodeForIdentifier(path->identifier,node->children);
                if (!nextNode)
                {
			nextNode = MyMallocEx(sizeof(OperClass_PathNode));
			nextNode->identifier = strdup(path->identifier);
			AddListItem(nextNode,node->children);
                }
                node = nextNode;
		path = path->next;
        }

	callbackNode = MyMallocEx(sizeof(OperClass_CallbackNode));
	callbackNode->callback = callback;
	callbackNode->parent = node;	
	AddListItem(callbackNode,node->callbacks);

	validator = MyMallocEx(sizeof(OperClassValidator));
	validator->node = callbackNode;	
        validator->owner = module;

	if (module)
        {
                ModuleObject *mobj = MyMallocEx(sizeof(ModuleObject));
                mobj->object.validator = validator;
                mobj->type = MOBJ_VALIDATOR;
                AddListItem(mobj, module->objects);
                module->errorcode = MODERR_NOERROR;
        }

	OperClass_freePath(path);

	return validator;
}

void OperClassValidatorDel(OperClassValidator* validator)
{
	if (validator->owner)
        {
                ModuleObject *mdobj;
                for (mdobj = validator->owner->objects; mdobj; mdobj = mdobj->next)
                {
                        if ((mdobj->type == MOBJ_VALIDATOR) && (mdobj->object.validator == validator))
                        {
                                DelListItem(mdobj, validator->owner->objects);
                                MyFree(mdobj);
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
	MyFree(validator->node);
	MyFree(validator);	
}

OperClassACLPath* OperClass_parsePath(char* path)
{
	char* pathCopy = strdup(path);
        OperClassACLPath* pathHead = NULL;
        OperClassACLPath* tmpPath;
        char *str = strtok(pathCopy,":");
        while (str)
        {
                tmpPath = MyMallocEx(sizeof(OperClassACLPath));
                tmpPath->identifier = strdup(str);
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

	MyFree(pathCopy);
        return pathHead;
}

void OperClass_freePath(OperClassACLPath* path)
{
	OperClassACLPath* next;
	while (path)
	{
		next = path->next;
		MyFree(path->identifier);
		MyFree(path);
		path = next;
	}	
}

OperClassACL* OperClass_FindACL(OperClassACL* acl, char* name)
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

OperClass_PathNode* OperClass_findPathNodeForIdentifier(char* identifier, OperClass_PathNode *head)
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

unsigned char OperClass_evaluateACLEntry(OperClassACLEntry* entry, OperClassACLPath* path, OperClassCheckParams* params)
{
	OperClass_PathNode *node = rootEvalNode;	
	OperClass_CallbackNode *callbackNode = NULL;
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

OperPermission ValidatePermissionsForPathEx(OperClassACL* acl, OperClassACLPath* path, OperClassCheckParams* params)
{
        /** Evaluate into ACL struct as deep as possible **/
	OperClassACLPath *basePath = path;
        OperClassACL* tmp;
        OperClassACLEntry* entry;
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

OperPermission ValidatePermissionsForPath(char* path, aClient *sptr, aClient *victim, aChannel *channel, void* extra)
{
	ConfigItem_oper *ce_oper;
        ConfigItem_operclass *ce_operClass;
        OperClass *oc = NULL;
        OperClassACLPath *operPath;
        OperClassACL *acl;

	if (!sptr)
		return OPER_DENY;

	/* Trust Servers, U:Lines and remote opers */
	if (IsServer(sptr) || IsULine(sptr) || (IsOper(sptr) && !MyClient(sptr)))
		return OPER_ALLOW;

	if (!IsOper(sptr))
		return OPER_DENY;

        ce_oper = Find_oper(sptr->user->operlogin);
	if (!ce_oper)
	{
		return OPER_DENY;
	}
	
	ce_operClass = Find_operclass(ce_oper->operclass);
        if (!ce_operClass)
        {
		return OPER_DENY;
        }

        oc = ce_operClass->classStruct;
        operPath = OperClass_parsePath(path);
        while (oc && operPath)
        {
                OperClassACL* acl = OperClass_FindACL(oc->acls,operPath->identifier);
                if (acl)
                {
                	OperPermission perm;
			OperClassCheckParams *params = MyMallocEx(sizeof(OperClassCheckParams));
        		params->sptr = sptr;
        		params->victim = victim;
        		params->channel = channel;
        		params->extra = extra;
			
                        perm = ValidatePermissionsForPathEx(acl, operPath, params);
			OperClass_freePath(operPath);
			MyFree(params);
			return perm;
                }
                if (!oc->ISA)
                {
                        break;
                }
                ce_operClass = Find_operclass(oc->ISA);
		if (ce_operClass)
		{
			oc = ce_operClass->classStruct;
		}
        }
	OperClass_freePath(operPath);
        return OPER_DENY;
}
