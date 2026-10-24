import os
import pypdf
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT, TA_JUSTIFY
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

# Register TrueType fonts with full Polish diacritics
pdfmetrics.registerFont(TTFont('Arial', 'arial.ttf'))
pdfmetrics.registerFont(TTFont('Arial-Bold', 'arialbd.ttf'))
pdfmetrics.registerFont(TTFont('Arial-Italic', 'ariali.ttf'))

# Color palette precisely matching FL_MCH.odg / FL_MCH.pdf
C_TITLE = colors.HexColor('#0d1b2a')        # 24pt Bold
C_SUBTITLE = colors.HexColor('#1b4965')     # 11pt Bold
C_HEADING = colors.HexColor('#0f2027')      # 13pt Bold
C_SUBHEADING = colors.HexColor('#1b4965')   # 10.5pt Bold
C_BODY = colors.HexColor('#1c2541')         # 9.5pt Normal
C_GRAY_TEXT = colors.HexColor('#7a8a9e')    # 8pt Normal
C_LINE = colors.HexColor('#2c3e50')         # Stroke line #2c3e50 (0.48 pt)
C_BOX_BORDER = colors.HexColor('#1b4965')   # Reference box outline

LOGO_PATH = os.path.abspath('docs/FL/LAB_ENG_001/Pictures/10000001000008AC0000043825DBCC12.png')
QR_PATH = os.path.abspath('docs/FL/LAB_ENG_001/Pictures/100000010000040400000404EF58CBB8.png')

class FLNumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_decorations(num_pages)
            super().showPage()
        super().save()

    def draw_page_decorations(self, page_count):
        self.saveState()

        # Top border line: from x=54 to x=541.02 at y=792
        self.setStrokeColor(C_LINE)
        self.setLineWidth(0.48)
        self.line(54, 792, 541.02, 792)

        # Bottom border line: from x=54 to x=541.02 at y=45
        self.line(54, 45, 541.02, 45)

        # Footer page number: "Strona X z Y" in Arial 8pt #7a8a9e
        self.setFont('Arial', 8)
        self.setFillColor(C_GRAY_TEXT)
        page_str = f"Strona {self._pageNumber} z {page_count}"
        self.drawRightString(541.02, 32, page_str)

        # Footer project logo (FL / LabEngine) on every page at bottom right: x=365.7, y=27.4, w=170, h=80.8
        if os.path.exists(LOGO_PATH):
            self.drawImage(LOGO_PATH, 365.7, 27.4, width=170.08, height=80.82, preserveAspectRatio=True, mask='auto')

        # Page 1 top-right badge / icon: x=479.1, y=729.9, w=50.5, h=50.5
        if self._pageNumber == 1 and os.path.exists(QR_PATH):
            self.drawImage(QR_PATH, 479.1, 729.9, width=50.54, height=50.54, preserveAspectRatio=True, mask='auto')

        self.restoreState()

def build_pdf():
    pdf_filename = "docs/FL/LAB_ENG_001/FL_Site-14.pdf"
    temp_filename = "docs/FL/LAB_ENG_001/FL_Site-14_temp.pdf"

    # Dimensions matching FL_MCH: A4, left=54pt (1.9cm), right=54pt, top=65pt, bottom=65pt
    doc = SimpleDocTemplate(
        temp_filename,
        pagesize=A4,
        leftMargin=54,
        rightMargin=54,
        topMargin=60,
        bottomMargin=65
    )

    style_title = ParagraphStyle(
        'DocTitle',
        fontName='Arial-Bold',
        fontSize=24,
        leading=28,
        textColor=C_TITLE,
        spaceAfter=3
    )

    style_subtitle = ParagraphStyle(
        'DocSubTitle',
        fontName='Arial-Bold',
        fontSize=11,
        leading=14,
        textColor=C_SUBTITLE,
        spaceAfter=14
    )

    style_h1 = ParagraphStyle(
        'SectionH1',
        fontName='Arial-Bold',
        fontSize=13,
        leading=16,
        textColor=C_HEADING,
        spaceBefore=14,
        spaceAfter=6
    )

    style_h2 = ParagraphStyle(
        'SectionH2',
        fontName='Arial-Bold',
        fontSize=10.5,
        leading=14,
        textColor=C_SUBHEADING,
        spaceBefore=8,
        spaceAfter=4
    )

    style_body = ParagraphStyle(
        'DocBody',
        fontName='Arial',
        fontSize=9.5,
        leading=13.8,
        textColor=C_BODY,
        spaceAfter=6,
        alignment=TA_LEFT
    )

    style_body_scenario = ParagraphStyle(
        'DocBodyScenario',
        fontName='Arial',
        fontSize=9.5,
        leading=14.0,
        textColor=C_BODY,
        spaceAfter=6,
        alignment=TA_JUSTIFY
    )

    style_bullet = ParagraphStyle(
        'DocBullet',
        fontName='Arial',
        fontSize=9.5,
        leading=13.8,
        textColor=C_BODY,
        leftIndent=14,
        firstLineIndent=-10,
        spaceAfter=4,
        alignment=TA_LEFT
    )

    style_dialogue = ParagraphStyle(
        'DocDialogue',
        fontName='Arial-Italic',
        fontSize=9.5,
        leading=13.8,
        textColor=C_BODY,
        leftIndent=20,
        spaceAfter=4
    )

    style_dialogue_speaker = ParagraphStyle(
        'DocDialogueSpeaker',
        fontName='Arial-Bold',
        fontSize=9.5,
        leading=13.8,
        textColor=C_SUBHEADING,
        leftIndent=20,
        spaceBefore=5,
        spaceAfter=1
    )

    style_box_title = ParagraphStyle(
        'BoxTitle',
        fontName='Arial-Bold',
        fontSize=10,
        leading=13,
        textColor=C_SUBHEADING,
        alignment=TA_CENTER,
        spaceAfter=3
    )

    style_box_text = ParagraphStyle(
        'BoxText',
        fontName='Arial',
        fontSize=8,
        leading=11,
        textColor=C_GRAY_TEXT,
        alignment=TA_CENTER
    )

    style_box_desc_left = ParagraphStyle(
        'BoxDescLeft',
        fontName='Arial',
        fontSize=8.5,
        leading=12,
        textColor=C_BODY,
        spaceAfter=3
    )

    story = []

    def make_ref_box(tag, subtitle_lines, height_pts=140):
        content = [
            Spacer(1, 10),
            Paragraph(f"<b>[ {tag} ]</b>", style_box_title),
            Spacer(1, 4)
        ]
        for line in subtitle_lines:
            content.append(Paragraph(line, style_box_text))
            content.append(Spacer(1, 2))
        
        t = Table([[content]], colWidths=[487.02], rowHeights=[height_pts])
        t.setStyle(TableStyle([
            ('BOX', (0,0), (-1,-1), 0.99, C_LINE),
            ('TOPPADDING', (0,0), (-1,-1), 0),
            ('BOTTOMPADDING', (0,0), (-1,-1), 0),
            ('LEFTPADDING', (0,0), (-1,-1), 12),
            ('RIGHTPADDING', (0,0), (-1,-1), 12),
            ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ]))
        return t

    def make_ref_box_two_col(tag1, lines1, tag2, lines2, height_pts=135):
        c1 = [
            Spacer(1, 8),
            Paragraph(f"<b>[ {tag1} ]</b>", style_box_title),
            Spacer(1, 4)
        ]
        for l in lines1:
            c1.append(Paragraph(l, style_box_text))
            c1.append(Spacer(1, 2))

        c2 = [
            Spacer(1, 8),
            Paragraph(f"<b>[ {tag2} ]</b>", style_box_title),
            Spacer(1, 4)
        ]
        for l in lines2:
            c2.append(Paragraph(l, style_box_text))
            c2.append(Spacer(1, 2))

        t = Table([[c1, c2]], colWidths=[238.5, 238.5], rowHeights=[height_pts])
        t.setStyle(TableStyle([
            ('BOX', (0,0), (0,0), 0.99, C_LINE),
            ('BOX', (1,0), (1,0), 0.99, C_LINE),
            ('TOPPADDING', (0,0), (-1,-1), 0),
            ('BOTTOMPADDING', (0,0), (-1,-1), 0),
            ('LEFTPADDING', (0,0), (-1,-1), 8),
            ('RIGHTPADDING', (0,0), (-1,-1), 8),
            ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ]))
        return t

    # ================= PAGE 1 =================
    # Title & Subtitle
    story.append(Paragraph("SITE-14: SEKTOR ENERGETYCZNY", style_title))
    story.append(Paragraph("DOKUMENTACJA LOKACJI I SCENARIUSZ DOJAZDU // ALASKA, 2032", style_subtitle))
    story.append(HRFlowable(width="100%", thickness=1.47, color=C_SUBTITLE, spaceBefore=0, spaceAfter=14))

    # Top metadata block (width matching FL_MCH left column: 275pt, while top right has box/qr)
    meta_left = [
        Paragraph("<b>Lokalizacja:</b> Odcięty płaskowyż arktyczny w głębi Alaski, pasmo Brooks Range (strefa wiecznej zmarzliny).", style_body),
        Paragraph("<b>Główny punkt wejścia:</b> Zabezpieczona szosa zaopatrzeniowa MP, tunel wlotowy do podziemnego kompleksu Site-14.", style_body),
        Paragraph("<b>Osiedle w oddali:</b> Zamknięte miasto-satelita pracownicze 'New Hope' widoczne na horyzoncie z przełęczy.", style_body),
        Paragraph("<b>Warunki wyjściowe:</b> Zwykły, mroźny poranek arktyczny (-19°C), skrzypiący śnieg, błękitne niebo, dymy z kominów.", style_body),
        Paragraph("<b>Kontekst architektoniczny:</b> Surowość alaskańskiej tajgi zderzona z brutalizmem amerykańskiego sektora zamkniętego.", style_body),
    ]

    p1_box_content = [
        Spacer(1, 12),
        Paragraph("<b>[ MIEJSCE NA SZKIC / VISTA ]</b>", style_box_title),
        Spacer(1, 4),
        Paragraph("Szkic koncepcyjny widoku z szoferki vana:", style_box_text),
        Paragraph("Ośnieżona szosa, ściana czarnych sosen (tajga),", style_box_text),
        Paragraph("w tle na horyzoncie szare bloki New Hope", style_box_text),
        Paragraph("i magistrala rurociągu.", style_box_text),
    ]

    top_table = Table([[meta_left, p1_box_content]], colWidths=[280, 207])
    top_table.setStyle(TableStyle([
        ('BOX', (1,0), (1,0), 0.99, C_LINE),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('LEFTPADDING', (0,0), (0,0), 0),
        ('RIGHTPADDING', (0,0), (0,0), 10),
        ('LEFTPADDING', (1,0), (1,0), 8),
        ('RIGHTPADDING', (1,0), (1,0), 8),
        ('TOPPADDING', (0,0), (-1,-1), 0),
        ('BOTTOMPADDING', (0,0), (-1,-1), 0),
    ]))
    story.append(top_table)
    story.append(Spacer(1, 14))

    # Section 1
    story.append(Paragraph("1. ARCHITEKTURA I ROZWIĄZANIA ASSET-EFFICIENT (LAB ENGINE)", style_h1))
    story.append(Paragraph(
        "Zamiast renderować cały obszar i narzucać sztywną geometrię kompleksu, poziom celowo rozpoczyna się na "
        "leśnej drodze dojazdowej w sercu alaskańskiej tajgi. Działa to w pełnej synergii z silnikiem <b>Lab Engine</b> "
        "(OpenGL 4.5+ DSA, Blinn-Phong, brak TAA, natywne MSAA/FXAA): silnik przetwarza minimalną liczbę obiektów, "
        "maksymalizując wydajność oświetlenia, ostrość geometrii pni drzew i gęste cząsteczki zmrożonego śniegu.",
        style_body
    ))
    story.append(Paragraph("Kluczowe założenia sekwencji otwierającej:", style_h2))
    story.append(Paragraph("• <b>Sekwencja w szoferce vana:</b> Protagonista Bruce Blackwell (starszy technik kriogeniki) siedzi jako pasażer. Wycieraczki rytmicznie zgarniają śnieg, silnik diesla cicho dudni w kabinie.", style_bullet))
    story.append(Paragraph("• <b>Sylwetka 'New Hope' w oddali (Skybox/Vista):</b> Przez boczną szybę widać odległą panoramę osiedla pracowniczego w dolinie — geometryczne bryły prefabrykowanych bloków z lat 80. i maszty radiowe.", style_bullet))
    story.append(Paragraph("• <b>Magistrala rurociągów:</b> Wzdłuż szosy pojawia się potężny naziemny rurociąg chłodniczy na betonowych pylonach, biegnący prosto do skalnego wjazdu.", style_bullet))
    story.append(Paragraph("• <b>Punkty kontrolne Military Police:</b> Posterunek wartowniczy ze szlabanem i zaporami na skraju lasu, a następnie wielka betonowa śluza tunelu wcinającego się w głąb góry.", style_bullet))

    # ================= PAGE 2 =================
    story.append(PageBreak())
    story.append(Paragraph("2. SCENARIUSZ OTWARCIA: SEKWENCJA DOJAZDOWA (AKT 1)", style_h1))
    story.append(Paragraph(
        "Poniższa sekwencja stanowi reżyserski zapis narracji wprowadzającej gracza w świat Frozen-Life. "
        "Konstrukcja nie zakłada gotowego rzutu wnętrz placówki — prowadzi percepcję gracza od klaustrofobii pojazdu ku wejściu.",
        style_body
    ))
    story.append(Spacer(1, 4))

    story.append(Paragraph("SEKWENCJA 01: SZOSA WŚRÓD TAJGI // PORANEK // WNĘTRZE VANA TRANSPORTOWEGO", style_h2))
    story.append(Paragraph(
        "Ciemność. Głuchy, miarowy pomruk ośmiocylindrowego silnika diesla pracującego na niskich obrotach. "
        "Z głośnika w desce rozdzielczej sączy się ciche, zaszumione pasmo lokalnego radia pogodowego z Fairbanks. "
        "Rytmiczne szuranie zmrożonych gum wycieraczek zgarniających twardy, suchy śnieg.",
        style_body_scenario
    ))
    story.append(Paragraph("FADE IN:", style_dialogue_speaker))
    story.append(Paragraph(
        "Kamera w perspektywie pierwszej osoby (FPP) z oczu BRUCE'A BLACKWELLA (41). Bruce siedzi nieruchomo "
        "w ocieplanej, technicznej kurtce na fotelu pasażera. Przez porysowaną przednią szybę widać wijącą się "
        "wśród kilkunastometrowych czarnych świerków szosę nr 14. Ośnieżony asfalt połyskuje lodem w ostrym świetle arktycznego świtu.",
        style_body_scenario
    ))

    story.append(Paragraph("KIEROWCA (GŁOS SPOD ZIMOWEJ CZAPKI)", style_dialogue_speaker))
    story.append(Paragraph("— Dzisiaj minus dziewiętnaście. Wiatr od północy. Mówią, że nocą ma spaść jeszcze pół metra. Przynajmniej pod ziemią macie stałą temperaturę, co Blackwell?", style_dialogue))

    story.append(Paragraph(
        "Bruce nie odwraca głowy. Wpatruje się w boczną szybę, na której narasta koronkowy szron. Jego twarz zdradza "
        "głębokie, wieloletnie spłaszczenie emocjonalne (Syndrom Zimowania). Poprawia kołnierz i rzuca cicho w przestrzeń kabiny:",
        style_body_scenario
    ))

    story.append(Paragraph("BRUCE", style_dialogue_speaker))
    story.append(Paragraph("— Na kriogenice mamy minus dwieście siedemdziesiąt. Ciekły hel nie pyta o pogodę.", style_dialogue))

    story.append(Paragraph(
        "Kierowca parska krótko pod nosem i redukuje bieg. Van wspina się na strome wzniesienie pod granią skalną.",
        style_body_scenario
    ))
    story.append(Spacer(1, 4))

    story.append(Paragraph("SEKWENCJA 02: PANORAMA OSADY 'NEW HOPE' W DOLINIE", style_h2))
    story.append(Paragraph(
        "Droga wychodzi z gęstego lasu na otwartą półkę skalną. Korony drzew opadają w dół, odsłaniając rozległą kotlinę górską. "
        "W oddali, na dnie doliny, ukazuje się surowa sylwetka 'NEW HOPE' — zamkniętego osiedla pracowniczego. "
        "Zwarte rzędy bloków mieszkalnych z prefabrykowanego betonu z lat 80. Na dachach gruba czapa śniegu, "
        "z kominów centralnej ciepłowni unoszą się pionowe, gęste słupy pary wodnej. Na wysokich masztach radiowych "
        "pulsują powoli czerwone lampy ostrzegawcze. To odizolowany, surowy mikrokosmos, w którym żyją rodziny personelu technicznego.",
        style_body_scenario
    ))

    # ================= PAGE 3 =================
    story.append(PageBreak())
    story.append(Paragraph("3. BRAMA KONTROLNA I WJAZD W GŁĄB MASYWU GÓRSKIEGO", style_h1))
    story.append(Paragraph(
        "Ostatni odcinek trasy prowadzi bezpośrednio do portalu tunelu. Płynne przejście kamery eliminuje jakiekolwiek "
        "cięcie montażowe, budując ciągłość doświadczenia gracza.",
        style_body
    ))
    story.append(Spacer(1, 4))

    story.append(Paragraph("SEKWENCJA 03: POSTERUNEK ŻANDARMERII (MILITARY POLICE)", style_h2))
    story.append(Paragraph(
        "Las ponownie zamyka się wokół szosy, lecz po prawej stronie pojawia się monumentalna instalacja naziemna: "
        "dwumetrowej średnicy rurociąg chłodniczy osadzony na potężnych betonowych pylonach, biegnący równolegle do drogi. "
        "Van zwalnia przed zasypaną śniegiem placówką wartowniczą.",
        style_body_scenario
    ))
    story.append(Paragraph(
        "Zapory przeciwczołgowe, zasieki ze stalowego drutu i drewniana budka wartownicza, z której dymi rura piecyka. "
        "Dwóch wartowników w białych pałatkach zimowych i goglach podchodzi do wozu. "
        "Kierowca uchyla szybę — do wnętrza wpada mroźny powiew powietrza i woń spalin diesla. Krótki zgrzyt skanera identyfikatorów.",
        style_body_scenario
    ))

    story.append(Paragraph("WARTOWNIK MP", style_dialogue_speaker))
    story.append(Paragraph("— Blackwell, Bruce. Poziom Trzeci, Utrzymanie Ruchu. Brama dwa jest otwarta. Na podjeździe uważać na czarny lód.", style_dialogue))

    story.append(Paragraph(
        "Hydraulika szlabanu jęczy pod naporem mrozu. Samochód rusza wolno naprzód.",
        style_body_scenario
    ))
    story.append(Spacer(1, 4))

    story.append(Paragraph("SEKWENCJA 04: BETONOWY PORTAL TUNELU I ZAMKNIĘCIE ŚLUZY", style_h2))
    story.append(Paragraph(
        "Przed maską vana wyrasta monumentalna gardziel tunelu — ciężki, żelbetowy portal o surowej fakturze szalunku, "
        "wżynający się w litą, granitową skałę pasma Brooks Range. Nad wjazdem widnieje odlewany z brązu, pokryty zielonkawą patyną napis:",
        style_body_scenario
    ))
    story.append(Paragraph("DEPARTMENT OF ENERGY // RESEARCH SECTOR 14 // AUTHORIZED PERSONNEL ONLY", style_dialogue))
    story.append(Paragraph(
        "Pojazd wjeżdża w głąb tunelu. Za samochodem z głuchym uderzeniem i sykiem dekompresji opadają dwuskrzydłowe wrota "
        "przeciwciśnieniowe ze stali pancernej. Dzienne światło gaśnie bezpowrotnie. "
        "Wnętrze zalewa pulsujące, brudnożółte światło sodowych lamp przemysłowych. "
        "Dźwięk silnika zostaje zastąpiony przez potężny rezonans podziemnych wentylatorów wyciągowych. "
        "Van zjeżdża w dół betonowej pochylni ku podziemnemu garażowi flotowemu — gracz bez cięcia kamery przejmuje pełną kontrolę nad postacią.",
        style_body_scenario
    ))

    # ================= PAGE 4 =================
    story.append(PageBreak())
    story.append(Paragraph("4. TABLICE REFERENCYJNE ŚRODOWISKA (ENVIRONMENT MOODBOARD)", style_h1))
    story.append(Paragraph(
        "Poniższe boksy stanowią dedykowaną przestrzeń na docelowe szkice koncepcyjne, rendery 3D z silnika Lab Engine "
        "oraz wytyczne stylistyczne dla działu level designu i grafiki środowiskowej.",
        style_body
    ))
    story.append(Spacer(1, 8))

    # Box 1: Bird's eye view (lot ptaka) - large top box
    b1_lines = [
        "Szeroki kadr z wysokości 150-200 m nad korfieldem alaskańskiej tajgi.",
        "Wąska, zalodzona wstęga asfaltu przecinająca bezkres ciemnozielonych świerków.",
        "Równoległa magistrala rurociągu na betonowych pylonach oraz monumentalny portal tunelu w zboczu góry.",
        "Skala: miniaturowy van na drodze podkreśla bezmiar i surowość arktycznej pustki."
    ]
    story.append(make_ref_box("REF 01: PERSPEKTYWA Z LOTU PTAKA // SZOSA, TAJGA I BRAMA SITE-14", b1_lines, height_pts=155))
    story.append(Spacer(1, 14))

    # Boxes 2 & 3: two-column layout
    b2_lines = [
        "Widok z przełęczy górskiej na dolinę.",
        "Prefabrykowane bloki mieszkalne z lat 80.,",
        "pulsujące czerwone światła masztów,",
        "słupy pary wodnej nad zasypanym miastem."
    ]
    b3_lines = [
        "Masywny żelbet wżynający się w skałę Alaski,",
        "stalowe wrota ciśnieniowe, zapory MP,",
        "brudnożółte lampy sodowe we wnętrzu tunelu."
    ]
    story.append(make_ref_box_two_col(
        "REF 02: VISTA 'NEW HOPE' W DOLINIE", b2_lines,
        "REF 03: BRAMA I ŚLUZA TUNELU", b3_lines,
        height_pts=145
    ))

    doc.build(story, canvasmaker=FLNumberedCanvas)
    print("Unencrypted PDF built:", temp_filename)

    # Now encrypt with parent folder name 'LAB_ENG_001'
    reader = pypdf.PdfReader(temp_filename)
    writer = pypdf.PdfWriter()
    for page in reader.pages:
        writer.add_page(page)
    
    writer.encrypt("LAB_ENG_001")
    with open(pdf_filename, "wb") as f_out:
        writer.write(f_out)
    
    os.remove(temp_filename)
    print(f"Encrypted PDF written to {pdf_filename} (password: LAB_ENG_001)")

if __name__ == '__main__':
    build_pdf()
