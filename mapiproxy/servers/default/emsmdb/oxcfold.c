/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
   \file oxcfold.c

   \brief Folder object routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details Open a System or Special folder object.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request OpenFolder request
   \param response pointer to the OpenFolder response

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS RopOpenFolder_SystemSpecialFolder(TALLOC_CTX *mem_ctx, 
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct OpenFolder_req request,
							 struct OpenFolder_repl *response)
{
	/* Find parent record */
	/* Set parent record as basedn */
	/* Look for systemfolder given its FolderID */

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS RopOpenFolder_GenericFolder(TALLOC_CTX *mem_ctx,
						   struct emsmdbp_context *emsmdbp_ctx,
						   struct OpenFolder_req request,
						   struct OpenFolder_repl *response,
						   struct mapi_handles *parent)
{
	struct emsmdbp_object	*parent_object = NULL;
	void			*data;
	uint64_t		parent_fid;
	int			retval;
	uint32_t		context_id;

	/* Step 1. Retrieve the parent fid given the handle */
	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *) data;
	if (!parent_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder null object"));
		return MAPI_E_NO_SUPPORT;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchane_emsmdb: [OXCFOLD] OpenFolder wrong object type: 0x%x\n", parent_object->type));
		return MAPI_E_NO_SUPPORT;
	}
	parent_fid = parent_object->object.folder->folderID;
	context_id = parent_object->object.folder->contextID;

	/* Step 2. Open folder from mapistore */
	retval = mapistore_opendir(emsmdbp_ctx->mstore_ctx, context_id, parent_fid, request.folder_id);
	if (retval) return MAPI_E_NOT_FOUND;

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc OpenFolder (0x02) Rop. This operation opens an
   existing folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenFolder(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct OpenFolder_req		request;
	struct OpenFolder_repl		response;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object;
	uint32_t			handle;
	int				parentfolder = -1;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] OpenFolder (0x02)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = mapi_req->u.mapi_OpenFolder;
	response = mapi_repl->u.mapi_OpenFolder;

	mapi_repl->u.mapi_OpenFolder.HasRules = 0;
	mapi_repl->u.mapi_OpenFolder.IsGhosted = 0;

	/* Step 1. Retrieve parent handle in the hierarchy */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_systemfolder(parent, &parentfolder);

	switch (parentfolder) {
	case -1:
	case 0x0:
		/* system/special folder */
		DEBUG(0, ("Opening system/special folder\n"));
		retval = RopOpenFolder_SystemSpecialFolder(mem_ctx, emsmdbp_ctx, request, &response);
		mapi_repl->error_code = retval;
		break;
	default:
		/* handled by mapistore */
		DEBUG(0, ("Opening Generic folder\n"));
		retval = RopOpenFolder_GenericFolder(mem_ctx, emsmdbp_ctx, request, &response, parent);
		mapi_repl->error_code = retval;
		break;
	}

	*size += libmapiserver_RopOpenFolder_size(mapi_repl);

	/* Fill EcDoRpc_MAPI_REPL reply */
	if (!mapi_repl->error_code) {
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);

		object = emsmdbp_object_folder_init((TALLOC_CTX *)emsmdbp_ctx, emsmdbp_ctx, mapi_req, parent);
		if (object) {
			retval = mapi_handles_set_systemfolder(rec, object->object.folder->systemfolder);
			retval = mapi_handles_set_private_data(rec, object);
		}
		mapi_repl->opnum = mapi_req->opnum;
		mapi_repl->handle_idx = mapi_req->u.mapi_OpenFolder.handle_idx;

		handles[mapi_repl->handle_idx] = rec->handle;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetHierarchyTable (0x04) Rop. This operation gets
   the subfolder hierarchy table for a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetHierarchyTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetHierarchyTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetHierarchyTable(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*data;
	uint64_t		folderID;
	uint32_t		contextID;
	uint32_t		handle;
	int			parentfolder = -1;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetHierarchyTable (0x04)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_systemfolder(parent, &parentfolder);

	/* Initialize default empty GetHierarchyTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetHierarchyTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* GetHierarchyTable can only be called for mailbox/folder objects */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		folderID = object->object.mailbox->folderID;
		contextID = object->object.folder->contextID;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->object.folder->folderID;
		contextID = object->object.folder->contextID;
		break;
	default:
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	switch (parentfolder) {
	case -1:
	case 0x1:
		/* system/special folder */
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, folderID, 
						       &mapi_repl->u.mapi_GetHierarchyTable.RowCount);
		break;
	default:
		/* handled by mapistore */
		retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, contextID, folderID, 
						    &mapi_repl->u.mapi_GetHierarchyTable.RowCount);
		break;
	}

	/* Initialize Table object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	object = emsmdbp_object_table_init((TALLOC_CTX *)rec, emsmdbp_ctx, parent);
	if (object) {
		retval = mapi_handles_set_private_data(rec, object);
		object->object.table->denominator = mapi_repl->u.mapi_GetHierarchyTable.RowCount;
		object->object.table->ulType = EMSMDBP_TABLE_FOLDER_TYPE;
	}

	*size += libmapiserver_RopGetHierarchyTable_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetContentsTable (0x05) Rop. This operation get
   the content table of a container.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetContentsTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetContentsTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetContentsTable(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] GetContentsTable (0x05)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetContentsTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_GetContentsTable.RowCount = 0;

	*size += libmapiserver_RopGetContentsTable_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS EcDoRpc_RopCreateGenericFolder(struct emsmdbp_context *emsmdbp_ctx,
						      union LPTSTR folderName, uint32_t parentFolder,
						      enum FOLDER_FLAGS folderFlags,
						      uint64_t *folderID, uint8_t *isExisting)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct ldb_message              *msg;
	struct ldb_dn                   *basedn;
	uint64_t			fid = 0;
	char                            *dn;
	char				*fid_formatted;
	char				*parentfid_formatted;
	int				ret = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateGenericFolder\n"));

	/* Step 0. Determine if the folder already exists */
	if (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, parentFolder,
					 folderName.lpszA, &fid) == MAPI_E_SUCCESS) {
		/* this folder already exists */
		if ( folderFlags & OPEN_IF_EXISTS ) {
		  	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder Duplicate Folder\n"));
			*isExisting = true;
			*folderID = fid;
			return MAPI_E_SUCCESS;
		} else {
			DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder Duplicate Folder error\n"));
			return MAPI_E_COLLISION;
		}
	}

	mem_ctx = talloc_named(NULL, 0, "RopCreateGenericFolder");

	/* TODO:
	   - We need to pass more information to underlying functions
             so folder creation also include customizable folder
             comment, class and so on.
	 */

	/* Step 1. Retrieve the next available folderID */
	retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &fid);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 2. Create the folder LDB record for openchange.ldb */
	fid_formatted = talloc_asprintf(mem_ctx, "0x%016"PRIx64, fid);
	parentfid_formatted = talloc_asprintf(mem_ctx, "0x%016x", parentFolder);
	*folderID = fid;
	
	dn = talloc_asprintf(mem_ctx, "CN=%s,CN=%s,CN=%s,%s", fid_formatted, 
			     parentfid_formatted, emsmdbp_ctx->username,
			     ldb_dn_get_linearized(ldb_get_root_basedn(emsmdbp_ctx->oc_ctx)));
	basedn = ldb_dn_new(mem_ctx, emsmdbp_ctx->oc_ctx, dn);
	talloc_free(dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);
	
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(mem_ctx, basedn);
	ldb_msg_add_string(msg, "objectClass", "systemfolder");
	ldb_msg_add_string(msg, "cn", fid_formatted);
	ldb_msg_add_string(msg, "PidTagContentUnreadCount", "0");
	ldb_msg_add_string(msg, "PidTagContentCount", "0");
	ldb_msg_add_string(msg, "PidTagContainerClass", "IPF.Note");
	ldb_msg_add_string(msg, "PidTagAttrHidden", "0");
	ldb_msg_add_string(msg, "PidTagDisplayName", folderName.lpszA);
	ldb_msg_add_string(msg, "PidTagParentFolderId", parentfid_formatted);
	ldb_msg_add_string(msg, "PidTagFolderId", fid_formatted);
	ldb_msg_add_fmt(msg, "mapistore_uri", "sqlite:///usr/local/samba/private/mapistore/%s/%s.db", 
			emsmdbp_ctx->username, fid_formatted);
	ldb_msg_add_string(msg, "PidTagSubFolders", "0");
	ldb_msg_add_string(msg, "FolderType", "1");
	ldb_msg_add_fmt(msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->dn));

	ret = ldb_add(emsmdbp_ctx->oc_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(fid_formatted);
	talloc_free(parentfid_formatted);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc CreateFolder (0x1c) Rop. This operation creates a
   folder on the remote server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the CreateFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the CreateFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error

   \note We do not provide support for GhostInfo
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCreateFolder(TALLOC_CTX *mem_ctx,
						 struct emsmdbp_context *emsmdbp_ctx,
						 struct EcDoRpc_MAPI_REQ *mapi_req,
						 struct EcDoRpc_MAPI_REPL *mapi_repl,
						 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent = NULL;
	uint32_t		handle;
	uint64_t		parent_fid;
	uint64_t		fid = 0;
	struct emsmdbp_object	*parent_object = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*data;
	struct mapi_handles	*rec = NULL;
	uint32_t		folder_handle;
	uint8_t			isExisting = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder (0x1c)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Search for the parent FID using handles */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_handles_get_private_data(parent, &data);
	parent_object = (struct emsmdbp_object *)data;
	if (!parent_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder null object\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	if (parent_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder wrong object type: 0x%x\n", parent_object->type));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}
	parent_fid = parent_object->object.folder->folderID;
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] CreateFolder parent: 0x%"PRIx64"\n", parent_fid));
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Creating %s\n", mapi_req->u.mapi_CreateFolder.FolderName.lpszA));
	
	/* Step 2. Initialize default empty CreateFolder reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_CreateFolder.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->u.mapi_CreateFolder.folder_id = 0;
	mapi_repl->u.mapi_CreateFolder.IsExistingFolder = false;

	/* Step 3. Do effective work here */
	switch (mapi_req->u.mapi_CreateFolder.ulFolderType) {
	case FOLDER_GENERIC:
		mapi_repl->error_code = EcDoRpc_RopCreateGenericFolder(emsmdbp_ctx,
								       mapi_req->u.mapi_CreateFolder.FolderName,
								       parent_fid,
								       mapi_req->u.mapi_CreateFolder.ulFlags,
								       &fid, &isExisting);
		break;
	case FOLDER_SEARCH:
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] FOLDER_SEARCH not implemented\n"));
		break;
	default:
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] Unexpected folder type\n"));
	}

	if (isExisting) {
		mapi_repl->u.mapi_CreateFolder.IsExistingFolder = true;
		mapi_repl->u.mapi_CreateFolder.GhostUnion.GhostInfo.HasRules = false;
		mapi_repl->u.mapi_CreateFolder.GhostUnion.GhostInfo.IsGhosted = false;
	}

	*size += libmapiserver_RopCreateFolder_size(mapi_repl);

	if (!mapi_repl->error_code) {
		/* Set the created folder FID */
		mapi_repl->u.mapi_CreateFolder.folder_id = fid;

		/* Step 4. Create a new handle */
		folder_handle = handles[mapi_req->handle_idx];
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, folder_handle, &rec);
		handles[mapi_repl->handle_idx] = rec->handle;

		/* Step 5. Initialize the internal folder object */
		object = emsmdbp_object_folder_init((TALLOC_CTX *)rec, emsmdbp_ctx, mapi_req, parent);
		if (object) {
			retval = mapi_handles_set_private_data(rec, object);
		}
	}

	return retval;
}

static enum MAPISTATUS DoDeleteFolder(struct emsmdbp_context *emsmdbp_ctx,
				      uint64_t parent_fid, uint64_t fid,
				      uint8_t flags)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_dn			*dn;
	char				*dn_str;
	int				ret = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder parent FID: 0x%"PRIx64"\n", parent_fid));
	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder target FID: 0x%"PRIx64"\n", fid));

	mem_ctx = talloc_named(NULL, 0, "DoDeleteFolder");

	/* TODO:
		1. This only works on top level folders.
		2. We should be careful not to delete special folders
		3. We need to handle deleting associated folders and messages (based on the flags)
	*/
	dn_str = talloc_asprintf(mem_ctx, "CN=0x%016"PRIx64",CN=0x%016"PRIx64",CN=%s,%s",
				 fid, parent_fid, emsmdbp_ctx->username,
			         ldb_dn_get_linearized(ldb_get_root_basedn(emsmdbp_ctx->oc_ctx)));
	dn = ldb_dn_new(mem_ctx, emsmdbp_ctx->oc_ctx, dn_str);
	talloc_free(dn_str);
	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(dn), MAPI_E_BAD_VALUE, mem_ctx);

	ret = ldb_delete(emsmdbp_ctx->oc_ctx, dn);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc DeleteFolder (0x1d) Rop. This operation deletes a
   folder on the remote server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteFolder EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteFolder EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update
   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteFolder(TALLOC_CTX *mem_ctx,
						 struct emsmdbp_context *emsmdbp_ctx,
						 struct EcDoRpc_MAPI_REQ *mapi_req,
						 struct EcDoRpc_MAPI_REPL *mapi_repl,
						 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;
	void			*handle_priv_data;
	struct emsmdbp_object	*handle_object = NULL;
	uint64_t		parent_fid = 0;

	DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder (0x1d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* TODO: factor this out to be convenience API */
	/* Convert the handle index into a handle, and then get the folder id */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_handles_get_private_data(rec, &handle_priv_data);
	handle_object = (struct emsmdbp_object *)handle_priv_data;
	if (!handle_object) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder null object\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}

	if (handle_object->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(4, ("exchange_emsmdb: [OXCFOLD] DeleteFolder wrong object type: 0x%x\n", handle_object->type));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		return MAPI_E_SUCCESS;
	}
	parent_fid = handle_object->object.folder->folderID;

	/* Initialize default empty DeleteFolder reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->u.mapi_DeleteFolder.PartialCompletion = false;

	mapi_repl->error_code = DoDeleteFolder(emsmdbp_ctx, parent_fid,
					       mapi_req->u.mapi_DeleteFolder.FolderId,
					       mapi_req->u.mapi_DeleteFolder.DeleteFolderFlags);

	*size += libmapiserver_RopDeleteFolder_size(mapi_repl);

	return retval;
}


/**
   \details EcDoRpc DeleteMessage (0x1e) Rop. This operation (soft) deletes
   a message on the server.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteMessage(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] DeleteMessage (0x1e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

	*size += libmapiserver_RopDeleteMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

