/*
    KindlePDFViewer: MuPDF abstraction for Lua
    Copyright (C) 2011 Hans-Werner Hilse <hilse@web.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <libdjvu/ddjvuapi.h>

#include "blitbuffer.h"
#include "djvu.h"

typedef struct DjvuDocument {
	ddjvu_context_t *context;
	ddjvu_document_t *doc_ref;
	int pages;
} DjvuDocument;

typedef struct DjvuPage {
	int num;
	ddjvu_page_t *page_ref;
	ddjvu_pageinfo_t info;
	DjvuDocument *doc;
} DjvuPage;

typedef struct DrawContext {
	int rotate;
	double zoom;
	double gamma;
	int offset_x;
	int offset_y;
	ddjvu_format_t *pixelformat;
} DrawContext;


static int handle(lua_State *L, ddjvu_context_t *ctx, int wait)
{
	const ddjvu_message_t *msg;
	if (!ctx)
		return;
	if (wait)
		msg = ddjvu_message_wait(ctx);
	while ((msg = ddjvu_message_peek(ctx)))
	{
	  switch(msg->m_any.tag)
		{
		case DDJVU_ERROR:
			if (msg->m_error.filename) {
				return luaL_error(L, "ddjvu: %s\nddjvu: '%s:%d'\n", 
					msg->m_error.message, msg->m_error.filename, 
					msg->m_error.lineno);
			} else {
				return luaL_error(L, "ddjvu: %s\n", msg->m_error.message);
			}
		default:
		  break;
		}
	  ddjvu_message_pop(ctx);
	}

	return 0;
}

static int openDocument(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	/*const char *password = luaL_checkstring(L, 2);*/

	DjvuDocument *doc = (DjvuDocument*) lua_newuserdata(L, sizeof(DjvuDocument));
	luaL_getmetatable(L, "djvudocument");
	lua_setmetatable(L, -2);

	doc->context = ddjvu_context_create("DJVUReader");
	if (! doc->context) {
		return luaL_error(L, "cannot create context.");
	}

	doc->doc_ref = ddjvu_document_create_by_filename(doc->context, filename, TRUE);
	while (! ddjvu_document_decoding_done(doc->doc_ref))
		handle(L, doc->context, True);
	if (! doc->doc_ref) {
		return luaL_error(L, "cannot open DJVU file <%s>", filename);
	}

	doc->pages = ddjvu_document_get_pagenum(doc->doc_ref);
	return 1;
}

static int closeDocument(lua_State *L) {
	DjvuDocument *doc = (DjvuDocument*) luaL_checkudata(L, 1, "djvudocument");
	if(doc->doc_ref != NULL) {
		ddjvu_document_release(doc->doc_ref);
		doc->doc_ref = NULL;
	}
	if(doc->context != NULL) {
		ddjvu_context_release(doc->context);
		doc->context = NULL;
	}
}

static int getNumberOfPages(lua_State *L) {
	DjvuDocument *doc = (DjvuDocument*) luaL_checkudata(L, 1, "djvudocument");
	lua_pushinteger(L, doc->pages);
	return 1;
}

static int newDrawContext(lua_State *L) {
	int rotate = luaL_optint(L, 1, 0);
	double zoom = luaL_optnumber(L, 2, (double) 1.0);
	int offset_x = luaL_optint(L, 3, 0);
	int offset_y = luaL_optint(L, 4, 0);
	double gamma = luaL_optnumber(L, 5, (double) -1.0);

	DrawContext *dc = (DrawContext*) lua_newuserdata(L, sizeof(DrawContext));
	dc->rotate = rotate;
	dc->zoom = zoom;
	/*dc->offset_x = offset_x;*/
	/*dc->offset_y = offset_y;*/
	dc->gamma = gamma;

	/*dc->pixelformat = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 4, format_mask);*/
	dc->pixelformat = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, NULL);
	ddjvu_format_set_row_order(dc->pixelformat, 1);
	ddjvu_format_set_y_direction(dc->pixelformat, 1);
	/*ddjvu_format_set_ditherbits(dc->pixelformat, 2);*/

	luaL_getmetatable(L, "drawcontext");
	lua_setmetatable(L, -2);

	return 1;
}

static int dcSetOffset(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	dc->offset_x = luaL_checkint(L, 2);
	dc->offset_y = luaL_checkint(L, 3);
	return 0;
}

static int dcGetOffset(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	lua_pushinteger(L, dc->offset_x);
	lua_pushinteger(L, dc->offset_y);
	return 2;
}

static int dcSetRotate(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	dc->rotate = luaL_checkint(L, 2);
	return 0;
}

static int dcSetZoom(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	dc->zoom = luaL_checknumber(L, 2);
	return 0;
}

static int dcGetRotate(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	lua_pushinteger(L, dc->rotate);
	return 1;
}

static int dcGetZoom(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	lua_pushnumber(L, dc->zoom);
	return 1;
}

static int dcSetGamma(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	dc->gamma = luaL_checknumber(L, 2);
	return 0;
}

static int dcGetGamma(lua_State *L) {
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 1, "drawcontext");
	lua_pushnumber(L, dc->gamma);
	return 1;
}

static int openPage(lua_State *L) {
	DjvuDocument *doc = (DjvuDocument*) luaL_checkudata(L, 1, "djvudocument");
	int pageno = luaL_checkint(L, 2);

	if(pageno < 1 || pageno > doc->pages) {
		return luaL_error(L, "cannot open page #%d, out of range (1-%d)", pageno, doc->pages);
	}

	DjvuPage *page = (DjvuPage*) lua_newuserdata(L, sizeof(DjvuPage));
	luaL_getmetatable(L, "djvupage");
	lua_setmetatable(L, -2);

	page->page_ref = ddjvu_page_create_by_pageno(doc->doc_ref, pageno - 1);
	while (! ddjvu_page_decoding_done(page->page_ref))
		handle(L, doc->context, TRUE);
	if(! page->page_ref) {
		return luaL_error(L, "cannot open page #%d", pageno);
	}

	page->doc = doc;
	page->num = pageno;

	/* @TODO:handle failure here */
	ddjvu_document_get_pageinfo(doc->doc_ref, pageno, &(page->info));

	return 1;
}

static int getPageSize(lua_State *L) {
	/*fz_matrix ctm;*/
	/*fz_rect bbox;*/
	DjvuPage *page = (DjvuPage*) luaL_checkudata(L, 1, "djvupage");
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 2, "drawcontext");

	/*ctm = fz_translate(0, -page->page->mediabox.y1);*/
	/*ctm = fz_concat(ctm, fz_scale(dc->zoom, -dc->zoom));*/
	/*ctm = fz_concat(ctm, fz_rotate(page->page->rotate));*/
	/*ctm = fz_concat(ctm, fz_rotate(dc->rotate));*/
	/*bbox = fz_transform_rect(ctm, page->page->mediabox);*/
	
	/*lua_pushnumber(L, bbox.x1-bbox.x0);*/
	/*lua_pushnumber(L, bbox.y1-bbox.y0);*/

	/*lua_pushnumber(L, page->info.width);*/
	/*lua_pushnumber(L, page->info.height);*/

	lua_pushnumber(L, ddjvu_page_get_width(page->page_ref) * dc->zoom);
	lua_pushnumber(L, ddjvu_page_get_height(page->page_ref) * dc->zoom);

	return 2;
}

/*static int getUsedBBox(lua_State *L) {*/
	/*fz_bbox result;*/
	/*fz_matrix ctm;*/
	/*fz_device *dev;*/
	/*DjvuPage *page = (DjvuPage*) luaL_checkudata(L, 1, "djvupage");*/

	/*[> returned BBox is in centi-point (n * 0.01 pt) <]*/
	/*ctm = fz_scale(100, 100);*/
	/*ctm = fz_concat(ctm, fz_rotate(page->page->rotate));*/

	/*fz_try(page->doc->context) {*/
		/*dev = fz_new_bbox_device(page->doc->context, &result);*/
		/*pdf_run_page(page->doc->xref, page->page, dev, ctm, NULL);*/
	/*}*/
	/*fz_always(page->doc->context) {*/
		/*fz_free_device(dev);*/
	/*}*/
	/*fz_catch(page->doc->context) {*/
		/*return luaL_error(L, "cannot calculate bbox for page");*/
	/*}*/

           /*lua_pushnumber(L, ((double)result.x0)/100);*/
	/*lua_pushnumber(L, ((double)result.y0)/100);*/
           /*lua_pushnumber(L, ((double)result.x1)/100);*/
	/*lua_pushnumber(L, ((double)result.y1)/100);*/

	/*return 4;*/
/*}*/

static int closePage(lua_State *L) {
	DjvuPage *page = (DjvuPage*) luaL_checkudata(L, 1, "djvupage");
	/*if(page->page != NULL) {*/
		/*pdf_free_page(page->doc->xref, page->page);*/
		/*page->page = NULL;*/
	/*}*/
	return 0;
}

static int drawPage(lua_State *L) {
	/*fz_pixmap *pix;*/
	/*fz_device *dev;*/
	/*fz_matrix ctm;*/
	/*fz_bbox bbox;*/
	ddjvu_rect_t pagerect, renderrect;
	char imagebuffer[600*800+1];
	memset(imagebuffer, 0, 600*800+1);


	DjvuPage *page = (DjvuPage*) luaL_checkudata(L, 1, "djvupage");
	DrawContext *dc = (DrawContext*) luaL_checkudata(L, 2, "drawcontext");
	BlitBuffer *bb = (BlitBuffer*) luaL_checkudata(L, 3, "blitbuffer");
	/*rect.x0 = luaL_checkint(L, 4);*/
	/*rect.y0 = luaL_checkint(L, 5);*/
	/*rect.x1 = rect.x0 + bb->w;*/
	/*rect.y1 = rect.y0 + bb->h;*/

	/* render page into rectangle specified by pagerect */
	pagerect.x = luaL_checkint(L, 4);
	pagerect.y = luaL_checkint(L, 5);
	pagerect.w = bb->w - pagerect.x;
	pagerect.h = bb->h - pagerect.y;

	/* copy pixels area from pagerect specified by renderrect */
	renderrect.x = dc->offset_x;
	renderrect.y = dc->offset_y;
	renderrect.w = bb->w - renderrect.x;
	renderrect.h = bb->h - renderrect.y;

	/*fz_clear_pixmap_with_value(page->doc->context, pix, 0xff);*/

	/*ctm = fz_scale(dc->zoom, dc->zoom);*/
	/*ctm = fz_concat(ctm, fz_rotate(page->page->rotate));*/
	/*ctm = fz_concat(ctm, fz_rotate(dc->rotate));*/
	/*ctm = fz_concat(ctm, fz_translate(dc->offset_x, dc->offset_y));*/
	/*dev = fz_new_draw_device(page->doc->context, pix);*/

	printf("%d, %d\n", bb->w, bb->h);
	ddjvu_page_render(page->page_ref,
			DDJVU_RENDER_COLOR,
			&pagerect,
			&renderrect,
			dc->pixelformat,
			bb->w,
			imagebuffer);

	/*if(dc->gamma >= 0.0) {*/
		/*fz_gamma_pixmap(page->doc->context, pix, dc->gamma);*/
	/*}*/

	uint8_t *bbptr = (uint8_t*)bb->data;
	uint8_t *pmptr = (uint8_t*)imagebuffer;
	int x, y;

	for(y = 0; y < bb->h; y++) {
		/* render every line */
		for(x = 0; x < (bb->w / 2); x++) {
			bbptr[x] = 255 - (((pmptr[x*2 + 1] & 0xF0) >> 4) | (pmptr[x*2] & 0xF0));
		}
		if(bb->w & 1) {
			bbptr[x] = 255 - (pmptr[x*2] & 0xF0);
		}
		/* go to next line */
		bbptr += bb->pitch;
		pmptr += bb->w;
	}

	return 0;
}

static const struct luaL_reg djvu_func[] = {
	{"openDocument", openDocument},
	{"newDC", newDrawContext},
	{NULL, NULL}
};

static const struct luaL_reg djvudocument_meth[] = {
	{"openPage", openPage},
	{"getPages", getNumberOfPages},
	/*{"getTOC", getTableOfContent},*/
	{"close", closeDocument},
	{"__gc", closeDocument},
	{NULL, NULL}
};

static const struct luaL_reg djvupage_meth[] = {
	{"getSize", getPageSize},
	/*{"getUsedBBox", getUsedBBox},*/
	{"close", closePage},
	{"__gc", closePage},
	{"draw", drawPage},
	{NULL, NULL}
};

static const struct luaL_reg drawcontext_meth[] = {
	{"setRotate", dcSetRotate},
	{"getRotate", dcGetRotate},
	{"setZoom", dcSetZoom},
	{"getZoom", dcGetZoom},
	{"setOffset", dcSetOffset},
	{"getOffset", dcGetOffset},
	{"setGamma", dcSetGamma},
	{"getGamma", dcGetGamma},
	{NULL, NULL}
};

int luaopen_djvu(lua_State *L) {
	luaL_newmetatable(L, "djvudocument");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, djvudocument_meth);
	lua_pop(L, 1);

	luaL_newmetatable(L, "djvupage");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, djvupage_meth);
	lua_pop(L, 1);

	luaL_newmetatable(L, "drawcontext");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, drawcontext_meth);
	lua_pop(L, 1);

	luaL_register(L, "djvu", djvu_func);
	return 1;
}
