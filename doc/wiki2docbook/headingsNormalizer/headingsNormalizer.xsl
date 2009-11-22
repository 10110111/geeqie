<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xhtml="http://www.w3.org/1999/xhtml"
	xmlns="http://www.w3.org/1999/xhtml">
  <xsl:output method="xml" indent="yes"/>

  <!-- 
    This stylesheet can be applied to xhtml documents. It ensures one one
    h1 element exists per document. If the input document has only one h1 
    element it just copies all nodes to the output, otherwise it adds a 
    new top level (h1) heading and depromotes every existing heading to a
    lower level (ie, h1s turn into h2s, h2s turn into h3s, etc).
  -->

  <xsl:param name="defaultHeading" select="'Chapter'"/>

  <xsl:template match="xhtml:html" mode="addtoplevelheading">
     <html>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
     </html>
  </xsl:template>

  <xsl:template match="xhtml:body" mode="addtoplevelheading">
    <body><h1><xsl:value-of select="$defaultTopHeading"/></h1>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </body>
  </xsl:template>

  <xsl:template match="xhtml:h1" mode="addtoplevelheading">
    <h2>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </h2>
  </xsl:template>

  <xsl:template match="xhtml:h2" mode="addtoplevelheading">
    <h3>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </h3>
  </xsl:template>

  <xsl:template match="xhtml:h3" mode="addtoplevelheading">
    <h4>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </h4>
  </xsl:template>

  <xsl:template match="xhtml:h4" mode="addtoplevelheading">
    <h5>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </h5>
  </xsl:template>

  <xsl:template match="xhtml:h5" mode="addtoplevelheading">
    <h6>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </h6>
  </xsl:template>

  <xsl:template match="xhtml:h6" mode="addtoplevelheading">
    <section>
	<xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </section>
  </xsl:template>

  <xsl:template match="@*|node()|text()|comment()|processing-instruction()" priority="-1" mode="addtoplevelheading">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="img[@alt]" priority="-1">
      <xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" />
  </xsl:template>

  <xsl:template match="@*|node()|text()|comment()|processing-instruction()" priority="-1">
    <xsl:choose>
      <xsl:when test="count(//*[local-name()='h1'])=1">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" />
    </xsl:copy>
      </xsl:when>
      <xsl:otherwise>
    <xsl:copy>
      <xsl:apply-templates select="@*|node()|text()|comment()|processing-instruction()" mode="addtoplevelheading"/>
    </xsl:copy>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>
