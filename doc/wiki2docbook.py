#!/usr/bin/python
#
# This script converts trac wiki to docbook
# wiki pages must be in wiki/ directory and their names must start with "Guide"
# the first page is named GuideIndex
# output is written to docbook/ directory
#
# based on the following scripts:
#
# http://trac-hacks.org/wiki/Page2DocbookPlugin
# http://trac.edgewall.org/attachment/wiki/TracWiki/trac_wiki2html.py
#
# see the links above for a list of requirements


import sys
import os
from trac.test import EnvironmentStub, Mock, MockPerm
from trac.mimeview import Context
from trac.wiki.formatter import HtmlFormatter
from trac.wiki.model import WikiPage
from trac.web.href import Href

import urllib
from tidy import parseString
import libxml2
import libxslt
import re

datadir = os.getcwd() + "/wiki2docbook"


xhtml2dbXsl = u"""<?xml version="1.0"?>
<xsl:stylesheet version="1.0" 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:import href=\"file:///""" + urllib.pathname2url(datadir + '/html2db/html2db.xsl') + """\" />
  <xsl:output method="xml" indent="no" encoding="utf-8"/>
  <xsl:param name="document-root" select="'__top_element__'"/>
</xsl:stylesheet>
"""

normalizedHeadingsXsl = u"""<?xml version="1.0"?>
<xsl:stylesheet version="1.0" 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:import href=\"file:///""" + urllib.pathname2url(datadir + '/headingsNormalizer/headingsNormalizer.xsl') + """\" />
  <xsl:output method="xml" indent="no" encoding="utf-8"/>
  <xsl:param name="defaultTopHeading" select="FIXME"/>
</xsl:stylesheet>
"""

normalizedHeadingsXsl_xmldoc = libxml2.parseDoc(normalizedHeadingsXsl)
normalizedHeadingsXsl_xsldoc = libxslt.parseStylesheetDoc(normalizedHeadingsXsl_xmldoc)

xhtml2dbXsl_xmldoc = libxml2.parseDoc(xhtml2dbXsl)
xhtml2dbXsl_xsldoc = libxslt.parseStylesheetDoc(xhtml2dbXsl_xmldoc)

def html2docbook(html):

	options = dict(output_xhtml=1, add_xml_decl=1, indent=1, tidy_mark=0, input_encoding='utf8', output_encoding='utf8', doctype='auto', wrap=0, char_encoding='utf8')
	xhtml = parseString(html.encode("utf-8"), **options)

	xhtml_xmldoc = libxml2.parseDoc(str(xhtml))

	xhtml2_xmldoc = normalizedHeadingsXsl_xsldoc.applyStylesheet(xhtml_xmldoc, None)

	nhstring = normalizedHeadingsXsl_xsldoc.saveResultToString(xhtml2_xmldoc)

	docbook_xmldoc = xhtml2dbXsl_xsldoc.applyStylesheet(xhtml2_xmldoc, None)

	dbstring = xhtml2dbXsl_xsldoc.saveResultToString(docbook_xmldoc)

	xhtml_xmldoc.freeDoc()
	xhtml2_xmldoc.freeDoc()
	docbook_xmldoc.freeDoc()
	return dbstring.decode('utf-8')


text = {}  #wiki text
depth = {} #document depth, 0 for index, leaf documents have depth 1 or 2
parent = {}#parent document (if depth > 0)
inner = {} #defined for documents that are parents

#top element indexed by depth
top_element = [ 'book', 'chapter', 'section', 'section', 'section', 'section', 'section', 'section', 'section', 'section' ]

env = EnvironmentStub()
req = Mock(href=Href('/'), abs_href=Href('http://www.example.com/'),
           authname='anonymous', perm=MockPerm(), args={})
context = Context.from_request(req, 'wiki')


def read_file(name):
	text[name] = file("wiki/" + name).read().decode('utf-8')
	page = WikiPage(env)
	page.name = name
	page.text = '--'
	page.save('', '', '::1', 0)


def read_index():
	index_name = "GuideIndex"
	read_file(index_name)
	index_text = text[index_name]
	depth[index_name] = 0
	inner[index_name] = 1
	
	stack = [ index_name , '', '', '' ]
	
	for line in index_text.splitlines() :
		match = re.match('^( *)\* \[wiki:(Guide[a-zA-Z0-9]*)', line)
		if match:
			name = match.group(2)
			d = len(match.group(1)) / 2
			if (d > 0):
				depth[name] = d
				parent[name] = stack[d - 1]
				inner[stack[d - 1]] = 1
				stack[d] = name
				read_file(name)

# exclude links with depth > 1 from wiki text, they will be included indirectly
def filter_out_indirect(text):
	out = ""
	for line in text.splitlines() :
		match = re.match('^( *)\* \[wiki:(Guide[a-zA-Z0-9]*)', line)
		d = 1
		if match:
			d = len(match.group(1)) / 2
		if (d == 1):
			 out = out + line + "\n"
	return out

def process_pages():
	for name in text.keys():
		txt = text[name]
		
		if name in inner:
			txt = filter_out_indirect(txt)
		
		html = HtmlFormatter(env, context, txt).generate()
		
		html = html.replace("/wiki/Guide", "#Guide")
		
		top = top_element[depth[name]]
		db = html2docbook(html)

		if name in inner:
			# replace list items with XIncludes, FIXME: this is ugly
			r = re.compile('<itemizedlist[^>]*>')
			db = r.sub(r'', db);
			
			r = re.compile('</itemizedlist>')
			db = r.sub(r'', db);
			
			r = re.compile('<listitem>\s*<para>\s*<link\s*linkend="(Guide[a-zA-Z0-9]*)">[^<]*</link>\s*</para>\s*</listitem>')
			db = r.sub(r'<xi:include xmlns:xi="http://www.w3.org/2001/XInclude" href="\1.xml"/>\n', db);
		
		
		db = db.replace("<__top_element__>", "<" + top + " id=\"" + name + "\">")
		db = db.replace("</__top_element__>", "</" + top + ">")
		
		open("docbook/" + name + ".xml", "w").write(db.encode('utf-8'))


read_index()
process_pages()



