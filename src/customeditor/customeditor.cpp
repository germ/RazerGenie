/*
 * Copyright (C) 2017-2018  Luca Weiss <luca (at) z3ntu (dot) xyz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "customeditor.h"
#include "config.h"
#include "util.h"
#include <QtWidgets>
#include <QPushButton>
#include <QHBoxLayout>
#include <QEvent>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>

CustomEditor::CustomEditor(libopenrazer::Device* device, bool launchMatrixDiscovery, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("RazerGenie - Custom Editor"));
    this->device = device;

    QVBoxLayout *vbox = new QVBoxLayout(this);

    dimens = device->getMatrixDimensions();
    qDebug() << dimens;

    // Initialize internal colors list
    for(int i=0; i<dimens[0]; i++) {
        colors << QVector<QColor>(dimens[1]);

        for(int j=0; j<dimens[1]; j++) {
            colors[i][j] = QColor(Qt::black);
        }
    }

    // Initialize selectedColor variable
    selectedColor = QColor(Qt::green);

    // Initialize drawStatus variable
    drawStatus = DrawStatus::set;

    // Add the main controls to the layout
    vbox->addLayout(generateMainControls());

    // Generate other buttons depending on the device type
    QString type = device->getDeviceType();

    // Generate matrix discovery if requested - ignore device type
    if(launchMatrixDiscovery) {
        vbox->addLayout(generateMatrixDiscovery());
    } else if(type == "keyboard") {
        if(dimens[0] == 6 && dimens[1] == 16) { // Razer Blade Stealth (Late 2017)
            if(!parseKeyboardJSON("razerblade16")) {
                closeWindow();
            }
        } else if(dimens[0] == 6 && dimens[1] == 22) { // "Normal" Razer keyboad (e.g. BlackWidow Chroma)
            if(!parseKeyboardJSON("razerdefault22")) {
                closeWindow();
            }
        } else if(dimens[0] == 6 && dimens[1] == 25) { // Razer Blade Pro 2017
            if(!parseKeyboardJSON("razerblade25")) {
                closeWindow();
            }
        } else {
            QMessageBox::information(0, tr("Unknown matrix dimensions"), tr("Please open an issue in the RazerGenie repository. Device name: %1 - matrix dimens: %2 %3").arg(device->getDeviceName()).arg(QString::number(dimens[0])).arg(QString::number(dimens[1])));
            closeWindow();
        }
        vbox->addLayout(generateKeyboard());
    } /*else if(type == "keypad") {
        vbox-addLayout(generateKeypad());
    } */else if(type == "mousemat") {
        if(dimens[0] == 1 && dimens[1] == 15) { // e.g. Firefly
            vbox->addLayout(generateMousemat());
        } else {
            QMessageBox::information(0, tr("Unknown matrix dimensions"), tr("Please open an issue in the RazerGenie repository. Device name: %1 - matrix dimens: %2 %3").arg(device->getDeviceName()).arg(QString::number(dimens[0])).arg(QString::number(dimens[1])));
            closeWindow();
        }
    } /*else if(type == "mouse") {
        vbox-addLayout(generateMouse());
    } */else {
        QMessageBox::information(0, tr("Device type not implemented!"), tr("Please open an issue in the RazerGenie repository. Device type: %1").arg(type));
        closeWindow();
    }

    // Set every LED to "off"/black unless loading
    // exisiting colormap, keyboards for now. Someone send me razer gear plz
    if (settings.value("exportToJSON").toBool() && type =="keyboard") {
      loadColours();
    } else {
      clearAll();
    }
}

CustomEditor::~CustomEditor()
{
  if (settings.value("exportToJSON").toBool()) {
    exportToJSON();
  }
}

void CustomEditor::closeWindow()
{
    setAttribute(Qt::WA_DeleteOnClose);
    this->close();
}

QLayout* CustomEditor::generateMainControls()
{
    QHBoxLayout *hbox = new QHBoxLayout();

    QPushButton *btnColor = new QPushButton();
    QPalette pal = btnColor->palette();
    pal.setColor(QPalette::Button, QColor(Qt::green));

    btnColor->setAutoFillBackground(true);
    btnColor->setFlat(true);
    btnColor->setPalette(pal);
    btnColor->setMaximumWidth(70);

    QPushButton *btnSet = new QPushButton(tr("Set"));
    QPushButton *btnClear = new QPushButton(tr("Clear"));
    QPushButton *btnClearAll = new QPushButton(tr("Clear All"));

    hbox->addWidget(btnColor);
    hbox->addWidget(btnSet);
    hbox->addWidget(btnClear);
    hbox->addWidget(btnClearAll);

    connect(btnColor, &QPushButton::clicked, this, &CustomEditor::colorButtonClicked);
    connect(btnSet, &QPushButton::clicked, this, &CustomEditor::setDrawStatusSet);
    connect(btnClear, &QPushButton::clicked, this, &CustomEditor::setDrawStatusClear);
    connect(btnClearAll, &QPushButton::clicked, this, &CustomEditor::clearAll);

    return hbox;
}

QLayout* CustomEditor::generateKeyboard()
{
    //TODO: Add missing logo button
    QVBoxLayout *vbox = new QVBoxLayout();
    QJsonObject keyboardLayout;
    bool found = false;
    QString kbdLayout = device->getKeyboardLayout();
    if(kbdLayout != "unknown" && keyboardKeys.contains(kbdLayout)) {
        keyboardLayout = keyboardKeys[kbdLayout].toObject();
    } else {
        if(kbdLayout == "unknown") {
            util::showInfo(tr("You are using a keyboard with a layout which is not known to the daemon. Please help us by visiting <a href='https://github.com/openrazer/openrazer/wiki/Keyboard-layouts'>https://github.com/openrazer/openrazer/wiki/Keyboard-layouts</a>. Using a fallback layout for now."));
        } else {
            util::showInfo(tr("Your keyboard layout (%1) is not yet supported by RazerGenie for this keyboard. Please open an issue in the RazerGenie repository.").arg(kbdLayout));
            closeWindow();
        }
        QStringList langs;
        langs << "de_DE" << "en_US" << "en_GB";
        QString lang;
        foreach(lang, langs) {
            if(keyboardKeys.contains(lang)) {
                keyboardLayout = keyboardKeys[lang].toObject();
                found = true;
                break;
            }
        }
        if(!found) {
            util::showInfo(tr("Neither one of these layouts was found in the layout file: %1. Exiting.").arg("de_DE, en_US, en_GB"));
            closeWindow();
        }
    }

    // Iterate over rows in the object
    QJsonObject::const_iterator it;
    for(it = keyboardLayout.constBegin(); it != keyboardLayout.constEnd(); ++it) {
        QJsonArray row = (*it).toArray();

        QHBoxLayout *hbox = new QHBoxLayout();
        hbox->setAlignment(Qt::AlignLeft);

        // Iterate over keys in row
        QJsonArray::const_iterator jt;
        for(jt = row.constBegin(); jt != row.constEnd(); ++jt) {
            QJsonObject obj = (*jt).toObject();

            if(!obj["label"].isNull()) {
                MatrixPushButton *btn = new MatrixPushButton(obj["label"].toString());
                int width = obj.contains("width") ? obj.value("width").toInt() : 60;
                int height = /*obj.contains("height") ? obj.value("height").toInt() : */63;
                btn->setFixedSize(width, height);
                if(obj.contains("matrix")) {
                    QJsonArray arr = obj["matrix"].toArray();
                    btn->setMatrixPos(arr[0].toInt(), arr[1].toInt());
                }
                if(obj.contains("disabled")) {
                    btn->setEnabled(false);
                }
                /*if(obj["cut"] == "enter") {
                    QPixmap pixmap("../../data/de_DE_mask.png");
                    btn->setMask(pixmap.mask());
                }*/
                connect(btn, &QPushButton::clicked, this, &CustomEditor::onMatrixPushButtonClicked);

                hbox->addWidget(btn);
                matrixPushButtons.append(btn);
            } else {
                QSpacerItem *spacer = new QSpacerItem(66, 69, QSizePolicy::Fixed, QSizePolicy::Fixed);
                hbox->addItem(spacer);
            }
        }
        vbox->addLayout(hbox);
    }
    return vbox;
}

QLayout* CustomEditor::generateMousemat()
{
    QHBoxLayout *hbox = new QHBoxLayout();
    // TODO: Improve visual style of the mousemat grid (make it look like the mousepad!)
    for(int i=0; i<dimens[1]; i++) {
        MatrixPushButton *btn = new MatrixPushButton(QString::number(i));
        btn->setMatrixPos(0, i);

        connect(btn, &QPushButton::clicked, this, &CustomEditor::onMatrixPushButtonClicked);

        hbox->addWidget(btn);
        matrixPushButtons.append(btn);
    }
    return hbox;
}

QLayout* CustomEditor::generateMouse()
{
    QHBoxLayout *hbox = new QHBoxLayout();
    return hbox;
}

QLayout* CustomEditor::generateMatrixDiscovery()
{
    QVBoxLayout *vbox = new QVBoxLayout();
    for(int i=0; i<dimens[0]; i++) {
        QHBoxLayout *hbox = new QHBoxLayout();
        for(int j=0; j<dimens[1]; j++) {
            MatrixPushButton *btn = new MatrixPushButton(QString::number(i) + "_" + QString::number(j));
            btn->setMatrixPos(i, j);

            connect(btn, &QPushButton::clicked, this, &CustomEditor::onMatrixPushButtonClicked);

            hbox->addWidget(btn);
            matrixPushButtons.append(btn);
        }
        vbox->addLayout(hbox);
    }
    return vbox;
}

bool CustomEditor::parseKeyboardJSON(QString jsonname)
{
    QFile *file; // Pointer to file object to use
    QFile file_devel("../../data/matrix_layouts/"+jsonname+".json"); // File during developemnt
    QFile file_prod(QString(RAZERGENIE_DATADIR) + "/matrix_layouts/"+jsonname+".json"); // File for production

    // Try to open the dev file (higher priority)
    if(file_devel.open(QIODevice::ReadOnly)) {
        qDebug() << "RazerGenie: Using the development "+jsonname+".json file.";
        file = &file_devel;
    } else {
        qDebug() << "RazerGenie: Development "+jsonname+".json failed to open. Trying the production location. Error: " << file_devel.errorString();

        // Try to open the production file
        if(file_prod.open(QIODevice::ReadOnly)) {
            file = &file_prod;
        } else {
            QMessageBox::information(0, tr("Error loading %1.json!").arg(jsonname), tr("The file %1.json, used for the custom editor failed to load: %2\nThe editor won't open now.").arg(jsonname).arg(file_prod.errorString()));
            return false;
        }
    }

    // Read the file
    QTextStream in(file);
    QString data = in.readAll();
    file->close();

    // Convert it to a QJsonObject
    keyboardKeys = QJsonDocument::fromJson(data.toUtf8()).object();

    return true;
}

bool CustomEditor::updateKeyrow(int row)
{
    return device->setKeyRow(row, 0, dimens[1]-1, colors[row]) && device->setCustom();
}

void CustomEditor::clearAll()
{
    QVector<QColor> blankColors;
    // Initialize the array with the width of the matrix with black = off
    for(int i=0; i<dimens[1]; i++) {
        blankColors << QColor(Qt::black);
    }

    // Send one request per row
    for(int i=0; i<dimens[0]; i++) {
        device->setKeyRow(i, 0, dimens[1]-1, blankColors);
    }

    device->setCustom();

    // Reset view
    for(int i=0; i<matrixPushButtons.size(); i++) {
        matrixPushButtons.at(i)->resetButtonColor();
    }

    // Reset model
    for(int i=0; i<colors.size(); i++) {
        for(int j=0; j<colors[i].size(); j++) {
            colors[i][j] = QColor(Qt::black);
        }
    }
}

void CustomEditor::colorButtonClicked()
{
    QPushButton *sender = qobject_cast<QPushButton*>(QObject::sender());

    QPalette pal(sender->palette());

    QColor oldColor = pal.color(QPalette::Button);

    QColor color = QColorDialog::getColor(oldColor);
    if(color.isValid()) {

        // Colorize the button
        pal.setColor(QPalette::Button, color);
        sender->setPalette(pal);

        // Set the color for other methods to use
        selectedColor = color;
    } else {
        qDebug() << "User cancelled the dialog.";
    }

}

void CustomEditor::onMatrixPushButtonClicked()
{
    MatrixPushButton *sender = dynamic_cast<MatrixPushButton*>(QObject::sender());
    QPair<int, int> pos = sender->matrixPos();
    if(drawStatus == DrawStatus::set) {
        // Set color in model
        colors[pos.first][pos.second] = selectedColor;
        // Set color in view
        sender->setButtonColor(selectedColor);
    } else if(drawStatus == DrawStatus::clear) {
        qDebug() << "Clearing color.";
        // Set color in model
        colors[pos.first][pos.second] = QColor(Qt::black);
        // Set color in view
        sender->resetButtonColor();
    } else {
        qDebug() << "RazerGenie: Unhandled DrawStatus: " << drawStatus;
    }
    // Set color on device
    updateKeyrow(pos.first);
}

void CustomEditor::setDrawStatusSet()
{
    drawStatus = DrawStatus::set;
}

void CustomEditor::setDrawStatusClear()
{
    drawStatus = DrawStatus::clear;
}

void CustomEditor::loadColours() {
  // TODO: Eventual plan, support multiple schemes via a pulldown switcher
  QString jsonString;
  QJsonDocument doc;
  QString devName;
  QFile file;
  QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/razergenie/colours/default.json";

  // Check for existing config
  file.setFileName(path);
  if (!file.exists()) {
    QMessageBox::information(0, 
      tr("Could not find scheme!"), 
      tr("Could not find : %1\n\n"
      "A new configuration will be generated.").arg(path));

    return;
  }

  // Check if device definitions are present
  file.open(QIODevice::ReadOnly | QIODevice::Text);
  jsonString = file.readAll();
  file.close();

  doc = QJsonDocument::fromJson(jsonString.toUtf8());
  if (doc.isNull()) {
    QMessageBox::information(0, 
      tr("Could not parse!"), 
      tr("Could not parse: %1\n\n"
      "Please check the document for errors.").arg(path));
    return;
  }

  config = doc.object();
  devName = device->getDeviceName();
  if (config[devName].isUndefined()) {
    // Will be added to colormap on save
    return;
  }

  // Awesome, we have a config
  QJsonObject devCfg = config[devName].toObject();
  qDebug()  << devCfg["Matrix"].toArray();
  abort();
  //for (i := 0; i < devCfg["Matrix"].toArray)
}

// Eventual plan, support multiple schemes via a pulldown switcher
// For now, just output to default.json, user can manage various outputs
// These outputs will be parsed by compatiable tooling for setting
// 
// A scheme contains definitions for multiple devices id'd by their product
// name (So configs can be shared). A user may have a scheme "solarized" that
// sets a keymap, mug color and mouse color. Or something. 
//
// Ideally we seperate colour defs from their raw values, instead using 
// variables. This would allow changing one value to effect all connected
// devices.
// TODO: Implement scheme switcher pulldown
// TODO: Add Vars section to JSON, to allow basic templating
void CustomEditor::exportToJSON() {
  QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/razergenie/colours/";
  QString jsonString;
  QFile file;
  QJsonObject json;
  QJsonArray matrix;

  // TODO: Support mice and other devices, can't test due to
  // Only having a Blackwidow :(
  // Idea here is to have various device properties stored
  // So a color for single colour things, text strings for
  // a scroller or whatever is needed. Only supports matrix 
  // Right now
  if (device->getDeviceType() != "keyboard") {
    return;
  }


  // Abandon all hope, JSON + QT is absolutely 100% verified terrible
  // Theres probably a better way to do this using generics or something
  QString devName = device->getDeviceName();
  dimens = device->getMatrixDimensions();
  json["Name"]    = "Default Profile";
  json["Author"]  = "RazerGenie";

  //Allocate matrix arrays
  for (int i = 0; i <= dimens[0]; i++) {
    QJsonArray ins;
    for (int j = 0; j < dimens[1]; j++) {
      ins.append("");
    }
    matrix.insert(i, ins);
  }

  // Populate matrix structure
  for (int i = 0; i < matrixPushButtons.length(); i++) {
    QPair<int,int> cords = matrixPushButtons[i]->matrixPos();
    QString keyColour = colors[cords.first][cords.second].name();
    QJsonArray ins = QJsonArray(matrix[cords.first].toArray());
    ins.insert(cords.second, keyColour);
    matrix[cords.first] = ins;
  }

  // Add device child to scheme
  QJsonObject devSection;
  devSection["Type"] = device->getDeviceType();
  devSection["Matrix"] = matrix;

  // Check if adding to existing config
  if (!config.isEmpty()) {
    config[devName] = devSection;
  } else {
    json[devName] = devSection;
    config = json;
  }

  // Make sure directory exists, write
  QDir dir(path);
  dir.mkpath(path);
  file.setFileName(path + "default.json");
  file.open(QIODevice::WriteOnly | QIODevice::Text);
  file.write(QJsonDocument(config).toJson());
  file.close();

  QMessageBox::information(0, 
    tr("Colormap exported!"),
    tr("Your colormap has been written to %1.json\n\n"
       "Please copy and rename this file if you do not want it to be overwritten!")
    .arg(path) ,
    tr("Close"));
}