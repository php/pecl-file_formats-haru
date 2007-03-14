<?php

$doc = new HaruDoc();
$p = $doc->addPage();

/* set the color (dark blue) */
$p->setRGBFill(0.2, 0.2, 0.5);
/* draw a rectangle */
$p->rectangle(150, 400, 300, 200);
/* fill it */
$p->fill();

/* se the color (white) */
$p->setRGBFill(1, 1, 1);

/* enter text mode */
$p->beginText();

/* choose the font and its size */
$font = $doc->getFont("Helvetica");
$p->setFontAndSize($font, 35);

/* move text position */
$p->moveTextPos(200,500);
/* print a well-known phrase */
$p->showText("Hello world!");

/* leave text mode */
$p->endText();

/* save the result */
$doc->save("/tmp/test.pdf"); 

?>
