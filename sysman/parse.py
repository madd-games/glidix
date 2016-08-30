#! /usr/bin/python
import sys, os

stylesheet = """
body
{
	font-family: monospace;
	background-color: #FFFFEE;
	font-size: 16px;
}

h1
{
	font-size: 20px;
	color: #990000;
}

h2
{
	font-size: 18px;
	color: #990000;
}

.code
{
	font-weight: bold;
	color: #660000;
}

.ref
{
	font-style: italic;
	font-weight: normal;
	color: #006600;
}

p, li
{
	margin-left: 1cm;
	margin-right: 1cm;
}

pre
{
	margin-left: 2cm;
	margin-right: 2cm;
	white-space: pre-wrap;       /* Since CSS 2.1 */
	white-space: -moz-pre-wrap;  /* Mozilla, since 1999 */
	white-space: -pre-wrap;      /* Opera 4-6 */
	white-space: -o-pre-wrap;    /* Opera 7 */
	word-wrap: break-word;       /* Internet Explorer 5.5+ */
}

div.headbar
{
	font-weight: bold;
	text-align: center;
}

a:link, a:visited
{
	color: #0022AA;
	text-decoration: none;
}

a:hover, a:active
{
	color: #EE0000;
	text-decoration: underline;
}
"""

os.system("rm -r out")
os.system("mkdir out")

def doEscapes(text):
	text = text.replace("&", "&amp;")
	text = text.replace("<", "&lt;")
	text = text.replace(">", "&gt;")
	return text

def parseParagraph(paragraph):
	output = ""
	while len(paragraph) != 0:
		if paragraph.startswith("\\"):
			output += paragraph[1]
			paragraph = paragraph[2:]
		elif paragraph.startswith("&"):
			output += "&amp;"
			paragraph = paragraph[1:]
		elif paragraph.startswith("<"):
			output += "&lt;"
			paragraph = paragraph[1:]
		elif paragraph.startswith(">"):
			output += "&gt;"
			paragraph = paragraph[1:]
		elif paragraph.startswith("*"):
			paragraph = paragraph[1:]
			endIndex = paragraph.find("*")
			text = doEscapes(paragraph[:endIndex])
			paragraph = paragraph[endIndex+1:]
			output += '<span class="code">%s</span>' % text
		elif paragraph.startswith("'"):
			paragraph = paragraph[1:]
			endIndex = paragraph.find("'")
			text = doEscapes(paragraph[:endIndex])
			paragraph = paragraph[endIndex+1:]
			output += '<span class="ref">%s</span>' % text
		elif paragraph.startswith("["):
			paragraph = paragraph[1:]
			endIndex = paragraph.find("]")
			page = paragraph[:endIndex]
			paragraph = paragraph[endIndex+1:]
			page, section = page.rsplit(".", 1)
			output += '<a href="%s.%s.html">%s(%s)</a>' % (page, section, page, section)
		else:
			output += paragraph[0]
			paragraph = paragraph[1:]
	return output

def parseCode(paragraph):
	output = ""
	while len(paragraph) != 0:
		if paragraph.startswith("\\"):
			output += paragraph[1]
			paragraph = paragraph[2:]
		elif paragraph.startswith("&"):
			output += "&amp;"
			paragraph = paragraph[1:]
		elif paragraph.startswith("<"):
			output += "&lt;"
			paragraph = paragraph[1:]
		elif paragraph.startswith(">"):
			output += "&gt;"
			paragraph = paragraph[1:]
		elif paragraph.startswith("'"):
			paragraph = paragraph[1:]
			endIndex = paragraph.find("'")
			text = paragraph[:endIndex]
			paragraph = paragraph[endIndex+1:]
			output += '<span class="ref">%s</span>' % text
		else:
			output += paragraph[0]
			paragraph = paragraph[1:]
	return output

chapters = {
	1:	[],
	2:	[],
	3:	[],
	4:	[],
	5:	[],
	6:	[]
}

for filename in os.listdir("src"):
	print "Generating HTML manual page for %s" % filename
	
	f = open("src/%s" % filename, "r")
	lines = f.read().splitlines()
	f.close()
	
	title, page = filename.split(".")
	page = int(page)
	chapters[page].append(title)
	
	f = open("out/%s.html" % filename, "w")
	f.write("<html>\n<head><title>%s(%d) - GLIDIX MANUAL</title>\n" % (title, page))
	f.write('<style type="text/css">\n')
	f.write(stylesheet)
	f.write("</style>\n</head>\n<body>\n")
	f.write("<div class=\"headbar\">GLIDIX MANUAL</div>\n<hr/>\n")
	
	try:
		it = iter(lines)
		while True:
			line = it.next()
			
			if line.startswith(">>"):
				f.write("<h2>%s</h2>\n" % line[2:])
			elif line.startswith(">"):
				f.write("<h1>%s</h1>\n" % line[1:])
			elif line.startswith("\\*"):
				f.write("<ul><li>%s</li></ul>" % parseParagraph(line[2:]))
			elif line.startswith("\t"):
				code = ""
				code += line[1:] + "\n"
				while True:
					line = it.next()
					if not line.startswith("\t"):
						break
					code += line[1:] + "\n"
				f.write('<pre class="code">%s</pre>\n' % parseCode(code))
			elif len(line) != 0:
				f.write("<p>%s</p>" % parseParagraph(line))
				
	except StopIteration:
		pass
	
	f.write("</body>\n</html>")
	f.close()

def writeChapter(f, num, caption):
	global chapters
	f.write("<li>%s</li>\n<ul>" % caption)
	for title in chapters[num]:
		f.write('<li><a href="%s.%d.html">%s</a></li>\n' % (title, num, title))
	f.write("</ul>\n")

print "Writing the table of contents..."
f = open("out/index.html", "w")
f.write("<html>\n<head><title>INDEX - GLIDIX MANUAL</title>\n")
f.write('<style type="text/css">\n')
f.write(stylesheet)
f.write("</style>\n</head>\n<body>\n")
f.write("<div class=\"headbar\">GLIDIX MANUAL</div>\n<hr/>\n")
f.write("<h1>TABLE OF CONTENTS</h1>\n")
f.write("<ol>\n")
writeChapter(f, 1, "Commands")
writeChapter(f, 2, "System calls and library functions")
writeChapter(f, 3, "Special files")
writeChapter(f, 4, "Kernel modules and drivers")
writeChapter(f, 5, "Kernel-mode API")
writeChapter(f, 6, "General concepts")
f.write("</ol>\n</body>\n</html>")
