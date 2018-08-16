.PHONY: *.tex language.pdf
.SUFFIXES : .tex .pdf .dvi .ps

FIGURES = $(patsubst %.dot,%.pdf,$(wildcard *.dot))
STDPDF = pdflatex language | grep -v "^Overfull"

default: rebuild

rebuild: $(FIGURES) language.pdf
	$(STDPDF)
	bibtex language
	$(STDPDF)
	$(STDPDF)

%.pdf: %.dot
	dot -o $@ -Tpdf $<

.tex.pdf: ${*}.tex
	pdflatex ${*}.tex
	bibtex ${*}
	pdflatex ${*}.tex
	pdflatex ${*}.tex

.tex.dvi: ${*}.tex
	latex ${*}.tex
	bibtex ${*}
	latex ${*}.tex
	latex ${*}.tex

clean:
	rm -f *.aux *.pdf std-gram.ext *.idx *.ilg *.ind *.log *.lot *.lof *.tmp *.out *.glo *.gls *.fls *.fdb* *.toc *.xtr language.bbl language.blg

#refresh:
#	$(STDPDF)
#full: $(FIGURES) reindex
#reindex:
#	$(STDPDF)
#	$(STDPDF)
#	$(STDPDF)
#	makeindex generalindex
#	makeindex headerindex
#	makeindex libraryindex
#	makeindex grammarindex
#	makeindex impldefindex
#	$(STDPDF)
#	makeindex -s basic.gst -o xrefindex.gls xrefindex.glo
#	makeindex -s basic.gst -o xrefdelta.gls xrefdelta.glo
#	$(STDPDF)
#	$(STDPDF)
