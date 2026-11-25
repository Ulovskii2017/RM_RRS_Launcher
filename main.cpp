#include <QApplication>
#include <QSettings>
#include <QPalette>
#include <QSize>
#include <QMainWindow>
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTextStream>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QListWidgetItem>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QFont>
#include <QDialog>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDomDocument>
#include <QSaveFile>
#include <QHash>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <functional>
#include <QPointer>
#include <QTimer>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QLocale>
#include <QComboBox>
#include <QCompleter>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QStackedLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QStyleOptionButton>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QIcon>
#include <QtMath>
#include <QPen>
#include <QStyle>
#include <QStyleHints>
#include <QStylePainter>
#include <QProxyStyle>
#include <QStringList>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>
#include <QVector2D>
#include <QOpenGLShader>
#include <cstring>
#include <functional>
#include <memory>
#include <QWheelEvent>
#include <QVector>
#include <QProcess>
#include <QTemporaryDir>
#include <QImage>
#include <QTcpSocket>
#include <QElapsedTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_2.h>
#endif





// Читает значение элемента внутри указанной секции XML.
static QString readXmlSectionValue(const QString &xml, const QString &section, const QString &tag,
                                   const QString &fallback = QString())
{
    const QRegularExpression sectionRx(QStringLiteral("<%1\\b[^>]*>([\\s\\S]*?)</%1>").arg(QRegularExpression::escape(section)));
    const auto sectionMatch = sectionRx.match(xml);
    if (!sectionMatch.hasMatch()) return fallback;
    const QRegularExpression valueRx(QStringLiteral("<%1\\b[^>]*>([\\s\\S]*?)</%1>").arg(QRegularExpression::escape(tag)));
    const auto valueMatch = valueRx.match(sectionMatch.captured(1));
    return valueMatch.hasMatch() ? valueMatch.captured(1).trimmed() : fallback;
}

// Обновляет только перечисленные элементы Viewer, сохраняя остальные данные settings.xml.
static bool writeViewerSettings(const QString &path, const QHash<QString, QString> &values,
                                QString *errorMessage = nullptr)
{
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось открыть %1: %2").arg(path, input.errorString());
        return false;
    }
    QString xml = QString::fromUtf8(input.readAll());
    input.close();

    const QRegularExpression viewerRx(QStringLiteral("(<Viewer\\b[^>]*>)([\\s\\S]*?)(</Viewer>)"));
    auto viewerMatch = viewerRx.match(xml);
    if (!viewerMatch.hasMatch()) {
        if (errorMessage) *errorMessage = QStringLiteral("В settings.xml не найдена секция Viewer.");
        return false;
    }
    QString viewerBody = viewerMatch.captured(2);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QString tag = QRegularExpression::escape(it.key());
        const QRegularExpression elementRx(QStringLiteral("(<%1\\b[^>]*>)([\\s\\S]*?)(</%1>)").arg(tag));
        auto elementMatch = elementRx.match(viewerBody);
        if (!elementMatch.hasMatch()) {
            if (errorMessage) *errorMessage = QStringLiteral("В секции Viewer не найден параметр <%1>.").arg(it.key());
            return false;
        }
        const QString replacement = elementMatch.captured(1) + it.value().toHtmlEscaped() + elementMatch.captured(3);
        viewerBody.replace(elementMatch.capturedStart(), elementMatch.capturedLength(), replacement);
    }
    xml.replace(viewerMatch.capturedStart(), viewerMatch.capturedLength(),
                viewerMatch.captured(1) + viewerBody + viewerMatch.captured(3));

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось записать %1: %2").arg(path, output.errorString());
        return false;
    }
    output.write(xml.toUtf8());
    if (!output.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось сохранить %1: %2").arg(path, output.errorString());
        return false;
    }
    return true;
}

// Обновляет только перечисленные элементы Launcher_settings, сохраняя остальные данные RM_launcher.xml.
static bool writeLauncherSettings(const QString &path, const QHash<QString, QString> &values,
                                  QString *errorMessage = nullptr)
{
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось открыть %1: %2").arg(path, input.errorString());
        return false;
    }
    QString xml = QString::fromUtf8(input.readAll());
    input.close();

    const QRegularExpression sectionRx(QStringLiteral("(<Launcher_settings\\b[^>]*>)([\\s\\S]*?)(</Launcher_settings>)"));
    const auto sectionMatch = sectionRx.match(xml);
    if (!sectionMatch.hasMatch()) {
        if (errorMessage) *errorMessage = QStringLiteral("В RM_launcher.xml не найдена секция Launcher_settings.");
        return false;
    }
    QString sectionBody = sectionMatch.captured(2);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QString tag = QRegularExpression::escape(it.key());
        const QRegularExpression elementRx(QStringLiteral("(<%1\\b[^>]*>)([\\s\\S]*?)(</%1>)").arg(tag));
        const auto elementMatch = elementRx.match(sectionBody);
        if (!elementMatch.hasMatch()) {
            if (errorMessage) *errorMessage = QStringLiteral("В Launcher_settings не найден параметр <%1>.").arg(it.key());
            return false;
        }
        const QString replacement = elementMatch.captured(1) + it.value().toHtmlEscaped() + elementMatch.captured(3);
        sectionBody.replace(elementMatch.capturedStart(), elementMatch.capturedLength(), replacement);
    }
    xml.replace(sectionMatch.capturedStart(), sectionMatch.capturedLength(),
                sectionMatch.captured(1) + sectionBody + sectionMatch.captured(3));

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось записать %1: %2").arg(path, output.errorString());
        return false;
    }
    output.write(xml.toUtf8());
    if (!output.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("Не удалось сохранить %1: %2").arg(path, output.errorString());
        return false;
    }
    return true;
}



// Runtime UI translation. Original Russian strings are retained in widget properties,
// so switching back and forth does not translate an already translated value.
static QString launcherTranslation(const QString &source, bool english)
{
    if (!english) return source;
    static const QHash<QString, QString> tr = {
        {"Главная", "Home"}, {"Мультиплеер", "Multiplayer"}, {"Загрузки", "Downloads"},
        {"Скриншоты", "Screenshots"}, {"Настройки", "Settings"}, {"Поезда", "Trains"},
        {"Конфигуратор поездов", "Train Configurator"}, {"Дополнения", "Add-ons"},
        {"Журнал", "Journal"}, {"Журналы", "Logs"}, {"Войти", "Sign in"},
        {"Никнейм", "Nickname"}, {"Запустить сервер", "Start server"}, {"Остановить сервер", "Stop server"},
        {"Запустить Viewer", "Launch Viewer"}, {"Запустить карту маршрута", "Launch route map"},
        {"Маршрут:", "Route:"}, {"Выберите маршрут...", "Select a route..."},
        {"В каталоге routes нет маршрутов", "No routes found in the routes directory"},
        {"Системное время", "System time"}, {"Время:", "Time:"},
        {"Выберите точку старта...", "Select a start point..."}, {"Прямое", "Forward"}, {"Обратное", "Reverse"},
        {"Выберите стартовую траекторию...", "Select a starting trajectory..."}, {"Выберите направление...", "Select a direction..."},
        {"Направление движения:", "Travel direction:"}, {"Точка старта:", "Starting point:"},
        {"Стартовая траектория:", "Starting trajectory:"}, {"Направление на стартовой траектории:", "Direction on starting trajectory:"},
        {"Координата:", "Coordinate:"}, {"Добавленные поезда", "Added trains"},
        {"Поиск по поездам...", "Search trains..."}, {"Подвижные единицы", "Rolling stock"},
        {"Поиск по единицам...", "Search rolling stock..."}, {"Все типы", "All types"},
        {"Локомотивы", "Locomotives"}, {"Пассажирские вагоны", "Passenger cars"},
        {"Грузовые вагоны", "Freight cars"}, {"Секции", "Sections"},
        {"Тип", "Type"}, {"Масса", "Mass"}, {"Масса груза", "Cargo mass"}, {"Длина", "Length"},
        {"Количество осей", "Number of axles"}, {"Тип автосцепок", "Coupler type"}, {"Диаметр колёс", "Wheel diameter"},
        {"Обратная ориентация", "Reverse orientation"}, {"На башмаках", "On wheel chocks"},
        {"＋  Добавить", "＋  Add"}, {"▤  Удалить", "▤  Remove"}, {"Состав поезда", "Train consist"},
        {"Информация о составе", "Consist information"}, {"⛓  Собрать новый поезд", "⛓  Build a new train"},
        {"✓  Сохранить изменения поезда", "✓  Save train changes"}, {"✎  Изменить параметры поезда", "✎  Edit train parameters"},
        {"▤  Удалить поезд", "▤  Delete train"}, {"Поезда не найдены", "No trains found"},
        {"Журналы логирования", "Logging logs"}, {"Отображать", "Display"}, {"Всю информацию", "All information"}, {"Информационные сообщения", "Information messages"}, {"Предупреждения", "Warnings"}, {"Ошибки", "Errors"}, {"Критические ошибки", "Critical errors"}, {"Содержимое TXT-файла выбранного журнала будет отображаться здесь.", "The selected log TXT file will be displayed here."},
        {"＋ Добавить сервер", "＋ Add server"}, {"Удалить сервер", "Remove server"},
        {"Доступные серверы не добавлены.", "No available servers have been added."},
        {"Официальный сервер RRS", "Official RRS server"}, {"Официальный сервер", "Official server"},
        {"Требуется авторизация", "Authentication required"}, {"Не проверен", "Not checked"},
        {"Обновить", "Refresh"}, {"Предыдущий скриншот", "Previous screenshot"},
        {"Следующий скриншот", "Next screenshot"}, {"Закрыть", "Close"},
        {"Основное", "General"}, {"Язык", "Language"}, {"Внешний вид", "Appearance"},
        {"Графика", "Graphics"}, {"Сеть", "Network"},
        {"Основные параметры лаунчера.", "General launcher settings."},
        {"Выбор языка интерфейса.", "Choose the interface language."},
        {"Настройки оформления и темы лаунчера.", "Launcher appearance and theme settings."},
        {"Параметры графики и отображения.", "Graphics and display settings."},
        {"Параметры сетевого подключения.", "Network connection settings."},
        {"Параметры запуска", "Launch options"}, {"Автоматический запуск вьювера", "Automatically launch Viewer"},
        {"Автоматический запуск карты маршрута", "Automatically launch route map"},
        {"Настройки Launcher", "Launcher settings"}, {"Язык лаунчера", "Launcher language"},
        {"Язык интерфейса", "Interface language"}, {"Русский", "Russian"}, {"Оформление лаунчера", "Launcher appearance"},
        {"Тёмная", "Dark"}, {"Светлая", "Light"}, {"Системная", "System"}, {"Тема", "Theme"},
        {"Стандартные", "Standard"}, {"Минималистичные", "Minimal"}, {"Классические", "Classic"}, {"Значки", "Icons"},
        {"Текущий графический процессор", "Current graphics processor"}, {"Предпочитаемый графический процессор", "Preferred graphics processor"},
        {"Автоматический выбор", "Automatic selection"}, {"Графические устройства не обнаружены", "No graphics devices detected"},
        {"Настройки окна", "Window settings"}, {"Разрешение", "Resolution"}, {"Полноэкранный режим", "Fullscreen"},
        {"Заголовок окна", "Window title"}, {"Номер монитора", "Monitor number"},
        {"Вертикальная синхронизация (V-Sync)", "Vertical synchronization (V-Sync)"},
        {"Ограничение частоты FPS", "FPS limit"}, {"Сглаживание", "Anti-aliasing"},
        {"Выключено", "Off"}, {"Настройки отображения", "Display settings"}, {"Дистанция видимости", "View distance"},
        {"Тени", "Shadows"}, {"Включить тени", "Enable shadows"}, {"Низкое", "Low"},
        {"Среднее", "Medium"}, {"Высокое", "High"}, {"Ультра", "Ultra"}, {"Пользовательское", "Custom"},
        {"Качество теней", "Shadow quality"}, {"Разрешение теней", "Shadow resolution"},
        {"Число каскадов теней", "Shadow cascade count"}, {"Дальность отрисовки теней", "Shadow render distance"},
        {"Настройки графики", "Graphics settings"}, {"Обновления", "Updates"},
        {"Проверять наличие обновлений автоматически", "Check for updates automatically"},
        {"Загрузка", "Downloads"}, {"Сервер загрузки", "Download server"},
        {"Определять автоматически", "Automatic"},
        {"Параметры сетевого подключения и взаимодействия с игровыми серверами.", "Network connection and game server settings."},
        {"Есть несохранённые изменения. Сохранить их перед переходом?", "There are unsaved changes. Save them before leaving?"},
        {"Несохранённые изменения", "Unsaved changes"}, {"Сохранить", "Save"}, {"Не сохранять", "Discard"},
        {"Отмена", "Cancel"}, {"Применить", "Apply"}, {"Папка симулятора не распознана", "Simulator folder not recognized"},
        {"Выберите корневую папку RRS.", "Select the RRS root folder."}, {"Выберите папку симулятора RRS", "Select the RRS simulator folder"},
        {"Раздел находится в разработке.", "This section is under development."},
        {"Учётная запись RRS", "RRS account"},
        {"Окно авторизации будет подключено после определения официального сервиса учётных записей RRS.", "The sign-in window will be connected once the official RRS account service is available."},
        {"Имя поезда", "Train name"}, {"Подготовить поезд", "Prepare train"}, {"Сохранение состава", "Saving consist"},
        {"Изменение поезда", "Edit train"}, {"Изменение имени и параметров поезда", "Edit train name and parameters"},
        {"Удаление поезда", "Delete train"}, {"Удалить поезд для удаления.", "Select a train to delete."},
        {"Имя файла не задано", "File name not specified"}, {"Файл уже существует", "File already exists"},
        {"Ошибка сохранения", "Save error"}, {"Ошибка удаления файла", "File deletion error"},
        {"Удаление отменено", "Deletion cancelled"}, {"Не удалось удалить конфигурацию поезда", "Could not delete train configuration"},
        {"Добавление поезда", "Add train"}, {"Сначала выберите поезд в поле «Поезд».", "First select a train in the Train field."},
        {"Выберите поезд в списке.", "Select a train from the list."}, {"Выберите поезд для удаления.", "Select a train to delete."},
        {"Тип состава", "Consist type"}, {"Количество единиц", "Number of vehicles"}, {"Общая длина", "Total length"},
        {"Общая масса", "Total mass"}, {"Масса поезда", "Train mass"}, {"Порожний", "Empty"},
        {"Пользовательское", "Custom"}, {"Настройки Launcher", "Launcher settings"},
        {"Не удалось прочитать конфигурацию Launcher:", "Could not read Launcher configuration:"},
        {"Не удалось прочитать файл настроек:", "Could not read settings file:"},
        {"Не удалось сохранить настройки:", "Could not save settings:"}, {"Выберите корректное разрешение окна.", "Select a valid window resolution."},
        {"Ошибка сохранения настроек", "Settings save error"}, {"Не удалось определить файл main.lua выбранного сценария.", "Could not locate main.lua for the selected scenario."},
        {"Сначала выберите маршрут.", "Select a route first."}, {"Сначала выберите сценарий.", "Select a scenario first."},
        {"Запуск сервера", "Start server"}, {"Запуск дополнительного приложения", "Launch companion application"},
        {"Запуск Viewer", "Launch Viewer"}, {"Карта маршрута", "Route map"},
        {"Добавить пользовательский сервер", "Add custom server"}, {"Название:", "Name:"}, {"Адрес / IP:", "Address / IP:"},
        {"Порт:", "Port:"}, {"Не заполнены данные", "Missing information"}, {"Укажите название и адрес сервера.", "Enter a server name and address."},
        {"Удалить сервер «%1» из списка?", "Remove server \"%1\" from the list?"},
        {"Выбран сервер «%1».", "Selected server: \"%1\"."}, {"Ping:", "Ping:"},
        {"Не удалось открыть файл журнала:", "Could not open log file:"}, {"Не удалось открыть файл журнала: ", "Could not open log file: "}, {"Папка logs не найдена", "Logs folder not found"},
        {"Обновления", "Updates"}, {"Yandex", "Yandex"}, {"Mail.ru", "Mail.ru"}, {"Google", "Google"},
        {"Дата старта:", "Start date:"}, {"Генерация свободной поездки", "Free ride generation"},
        {"Выберите поезд...", "Select a train..."}, {"Поезд:", "Train:"}, {"＋ Добавить", "＋ Add"}, {"− Удалить", "− Remove"},
        {"Выберите стартовую траекторию или точку старта.", "Select a starting trajectory or starting point."},
        {"Стартовая траектория «%1» уже занята другим поездом.", "Starting trajectory \"%1\" is already occupied by another train."},
        {"Выберите направление на стартовой траектории.", "Select a direction on the starting trajectory."},
        {"Укажите координату старта.", "Specify the starting coordinate."}, {"Выбор сценария", "Scenario selection"},
        {"Описание", "Description"}, {"Созданные поезда", "Created trains"}, {"ВЛ80с-1274", "VL80s-1274"},
        {"да", "yes"}, {"нет", "no"}, {"В папке cfg/trains не найдено корректных XML-файлов поездов.", "No valid train XML files found in cfg/trains."},
        {"Нет", "No"}, {"Есть", "Yes"}, {"нет данных", "no data"}, {" м", " m"}, {" т", " t"},
        {"Подвижная единица", "Rolling stock unit"}, {"Не найден ModelName. Конфиг: %1", "ModelName not found. Config: %1"},
        {"ModelName должен быть относительным путём: %1", "ModelName must be a relative path: %1"},
        {"не требуется", "not required"}, {"прочитан", "read"}, {"не найден / недоступен", "not found / unavailable"},
        {"Добавление единицы", "Add rolling stock unit"}, {"Выберите подвижную единицу в каталоге.", "Select a rolling stock unit from the catalog."},
        {"Удаление единицы", "Remove rolling stock unit"}, {"Выберите единицу в ленте состава.", "Select a unit in the consist list."},
        {"Введите название поезда...", "Enter train name..."}, {"Например: MyPassengerTrain", "For example: MyPassengerTrain"},
        {"Только английские буквы A–Z, цифры, дефис и подчёркивание. Без .xml", "Only A–Z letters, digits, hyphens and underscores. No .xml extension"},
        {"Введите описание поезда...", "Enter train description..."}, {"Поезд без воздуха", "Train without air"},
        {"Имя файла (латиница):", "File name (Latin characters):"}, {"Название поезда:", "Train name:"},
        {"Зарядное давление:", "Charging pressure:"}, {"Начальное давление:", "Initial pressure:"}, {"Описание:", "Description:"},
        {"Название не задано", "Name not specified"}, {"Введите название поезда.", "Enter a train name."},
        {"Некорректное имя файла", "Invalid file name"}, {"Расширение .xml добавляется автоматически.", "The .xml extension is added automatically."},
        {"Сборка нового поезда", "Build new train"}, {"Откройте «Изменить имя поезда» и укажите имя XML-файла латиницей.", "Open Edit train name and enter the XML file name using Latin characters."},
        {"Невозможно сохранить поезд: %1.xml уже существует.", "Cannot save train: %1.xml already exists."},
        {"Старый файл не удалён", "Old file was not deleted"}, {"Имя файла обязательно должно быть написано латинскими буквами.", "The file name must use Latin characters."},
        {"Невозможно применить изменения: файл %1.xml уже существует.", "Cannot apply changes: file %1.xml already exists."},
        {"Удалить поезд «%1»?", "Delete train \"%1\"?"},
        {"В поезде «%1» есть несохранённые изменения. Сохранить их перед переключением?", "Train \"%1\" has unsaved changes. Save them before switching?"},
        {"Оставить без изменений", "Leave unchanged"},
        {"Невозможно сохранить поезд: укажите корректное имя XML-файла латиницей через «Изменить имя поезда».", "Cannot save train: enter a valid Latin-character XML file name using Edit train name."},
        {"В папке logs нет файлов", "No files in the logs folder"}, {"Доступные серверы", "Available servers"},
        {"▶ Запустить Viewer", "▶ Launch Viewer"}, {"⌖ Запустить карту маршрута", "⌖ Launch route map"},
        {"Например, Сервер друзей", "For example, Friends' server"}, {"IP-адрес или доменное имя", "IP address or domain name"},
        {"Удаление сервера", "Remove server"}, {"В папке screenshots пока нет изображений", "There are no images in the screenshots folder yet."},
        {"не выбрано", "not selected"}, {"Сценарий RRS (Lua).", "RRS scenario (Lua)."},
        {"Russian Railway Simulator · Версия %1", "Russian Railway Simulator · Version %1"},
        {"Не удалось открыть %1: %2", "Could not open %1: %2"}, {"В settings.xml не найдена секция Viewer.", "Viewer section not found in settings.xml."},
        {"В секции Viewer не найден параметр <%1>.", "Parameter <%1> not found in the Viewer section."},
        {"Не удалось записать %1: %2", "Could not write %1: %2"}, {"Не удалось сохранить %1: %2", "Could not save %1: %2"},
        {"В RM_launcher.xml не найдена секция Launcher_settings.", "Launcher_settings section not found in RM_launcher.xml."},
        {"В Launcher_settings не найден параметр <%1>.", "Parameter <%1> not found in Launcher_settings."},
        {"Ошибка формирования XML-файла.", "Error creating XML file."}, {"В конфиге не найдена основная модель", "Main model not found in config"},
        {"Не удалось открыть GLTF: %1", "Could not open GLTF: %1"}, {"Некорректный GLTF: %1", "Invalid GLTF: %1"},
        {"В модели нет BIN-буфера", "Model has no BIN buffer"}, {"Не найден BIN: %1", "BIN not found: %1"},
        {"Геометрия не найдена", "Geometry not found"},
        {"Геометрия загружена · узлов: %1 · изображений: %2/%3 · материалов с текстурой: %4", "Geometry loaded · nodes: %1 · images: %2/%3 · textured materials: %4"},
        {"Выберите подвижную единицу", "Select a rolling stock unit"}, {"не определена", "not identified"},
        {"Маршрут не выбран", "No route selected"}, {" · кэш", " · cache"}
    };
    return tr.value(source, source);
}

static bool gLauncherEnglish = false;

static void applyLauncherLanguage(QWidget *root, bool english)
{
    if (!root) return;
    gLauncherEnglish = english;
    const auto widgets = root->findChildren<QWidget *>();
    QList<QWidget *> all; all << root; all.append(widgets);
    for (QWidget *w : all) {
        if (auto *b = qobject_cast<QAbstractButton *>(w)) {
            if (!w->property("rrsSourceText").isValid()) w->setProperty("rrsSourceText", b->text());
            b->setText(launcherTranslation(w->property("rrsSourceText").toString(), english));
        } else if (auto *g = qobject_cast<QGroupBox *>(w)) {
            if (!w->property("rrsSourceText").isValid()) w->setProperty("rrsSourceText", g->title());
            g->setTitle(launcherTranslation(w->property("rrsSourceText").toString(), english));
        } else if (auto *l = qobject_cast<QLabel *>(w)) {
            if (!w->property("rrsSourceText").isValid()) w->setProperty("rrsSourceText", l->text());
            l->setText(launcherTranslation(w->property("rrsSourceText").toString(), english));
        }
        if (auto *list = qobject_cast<QListWidget *>(w)) {
            for (int i=0;i<list->count();++i) {
                QListWidgetItem *item = list->item(i);
                const int sourceRole = Qt::UserRole + 97;
                if (!item->data(sourceRole).isValid()) item->setData(sourceRole, item->text());
                item->setText(launcherTranslation(item->data(sourceRole).toString(), english));
            }
        }
        if (auto *edit = qobject_cast<QLineEdit *>(w)) {
            if (!w->property("rrsSourcePlaceholder").isValid()) w->setProperty("rrsSourcePlaceholder", edit->placeholderText());
            edit->setPlaceholderText(launcherTranslation(w->property("rrsSourcePlaceholder").toString(), english));
        }
        const auto actions = w->findChildren<QAction *>();
        for (QAction *action : actions) {
            if (!action->property("rrsSourceText").isValid()) action->setProperty("rrsSourceText", action->text());
            action->setText(launcherTranslation(action->property("rrsSourceText").toString(), english));
        }
        if (auto *tabs = qobject_cast<QTabWidget *>(w)) {
            if (!w->property("rrsSourceTabs").isValid()) {
                QStringList labels; for (int i=0;i<tabs->count();++i) labels << tabs->tabText(i);
                w->setProperty("rrsSourceTabs", labels);
            }
            const QStringList labels = w->property("rrsSourceTabs").toStringList();
            for (int i=0;i<qMin(tabs->count(), labels.size());++i) tabs->setTabText(i, launcherTranslation(labels.at(i), english));
        }
        if (auto *c = qobject_cast<QComboBox *>(w)) {
            if (!w->property("rrsSourceItems").isValid()) {
                QStringList items; for (int i=0;i<c->count();++i) items << c->itemText(i);
                w->setProperty("rrsSourceItems", items);
            }
            const QStringList originals = w->property("rrsSourceItems").toStringList();
            const QSignalBlocker blocker(c);
            for (int i=0;i<qMin(c->count(), originals.size());++i)
                c->setItemText(i, launcherTranslation(originals.at(i), english));
        }
    }
    if (root->isWindow()) {
        if (!root->property("rrsSourceTitle").isValid()) root->setProperty("rrsSourceTitle", root->windowTitle());
        root->setWindowTitle(launcherTranslation(root->property("rrsSourceTitle").toString(), english));
    }
    if (auto *mw = qobject_cast<QMainWindow *>(root)) {
        if (mw->statusBar()) {
            if (!mw->statusBar()->property("rrsSourceMessage").isValid())
                mw->statusBar()->setProperty("rrsSourceMessage", mw->statusBar()->currentMessage());
            mw->statusBar()->showMessage(launcherTranslation(mw->statusBar()->property("rrsSourceMessage").toString(), english));
        }
    }
}

class LauncherLiveTranslationFilter : public QObject
{
protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            if (auto *widget = qobject_cast<QWidget *>(watched))
                applyLauncherLanguage(widget, gLauncherEnglish);
        }
        return QObject::eventFilter(watched, event);
    }
};

// Блокирует элементы генератора до выбора маршрута и показывает понятное уведомление.
class RouteSelectionGuard : public QObject
{
public:
    RouteSelectionGuard(QComboBox *routeSelector, QWidget *messageParent)
        : QObject(messageParent), m_routeSelector(routeSelector), m_messageParent(messageParent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!m_routeSelector || !m_routeSelector->currentData().toString().isEmpty())
            return QObject::eventFilter(watched, event);

        const QEvent::Type type = event->type();
        if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick ||
            type == QEvent::KeyPress || type == QEvent::Wheel || type == QEvent::FocusIn) {
            if (type != QEvent::FocusIn) {
                QMessageBox::information(m_messageParent, QStringLiteral("Маршрут не выбран"),
                    QStringLiteral("Сначала выберите маршрут.") );
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QComboBox> m_routeSelector;
    QWidget *m_messageParent = nullptr;
};

// Допустимое имя файла поезда: только латинские буквы, цифры, дефис и подчёркивание.
static bool isValidTrainFileName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return pattern.match(name).hasMatch();
}

// Записывает конфигурацию поезда в XML, совместимый со структурой,
// которую Launcher читает из cfg/trains.
static bool saveTrainConfig(const QString &path, const QString &title,
                            const QString &description, double chargingPressure,
                            double initMainResPressure, bool noAir,
                            const QVariantList &blocks, QString *errorMessage = nullptr)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("Config"));
    xml.writeStartElement(QStringLiteral("Common"));
    xml.writeTextElement(QStringLiteral("Title"), title);
    xml.writeTextElement(QStringLiteral("Description"), description);
    xml.writeTextElement(QStringLiteral("ChargingPressure"), QString::number(chargingPressure, 'g', 15));
    xml.writeTextElement(QStringLiteral("InitMainResPressure"), QString::number(initMainResPressure, 'g', 15));
    xml.writeTextElement(QStringLiteral("NoAir"), noAir ? QStringLiteral("1") : QStringLiteral("0"));
    xml.writeEndElement(); // Common

    // В редакторе каждая физическая подвижная единица хранится отдельной
    // записью. При формировании XML объединяем одинаковые соседние/повторные
    // записи в один Vehicle и увеличиваем Count.
    QVariantList groupedVehicles;
    QHash<QString, int> groupedIndex;
    const QStringList identityFields = {
        QStringLiteral("ModuleConfigDir"), QStringLiteral("ModuleConfig"),
        QStringLiteral("IsOrientationForward"), QStringLiteral("PayloadCoeff"),
        QStringLiteral("IsBrakeShoes")
    };
    for (const QVariant &entry : blocks) {
        QVariantMap vehicle = entry.toMap();
        if (vehicle.value("ModuleConfig").toString().trimmed().isEmpty()) continue;
        if (vehicle.value("IsOrientationForward").toString().isEmpty())
            vehicle["IsOrientationForward"] = QStringLiteral("1");
        if (vehicle.value("PayloadCoeff").toString().isEmpty())
            vehicle["PayloadCoeff"] = QStringLiteral("1");
        if (vehicle.value("IsBrakeShoes").toString().isEmpty())
            vehicle["IsBrakeShoes"] = QStringLiteral("0");

        QString key;
        for (const QString &field : identityFields)
            key += vehicle.value(field).toString() + QChar(0x1f);
        const int count = qMax(1, vehicle.value("Count", 1).toInt());
        if (groupedIndex.contains(key)) {
            const int index = groupedIndex.value(key);
            QVariantMap existing = groupedVehicles[index].toMap();
            existing["Count"] = existing.value("Count", 1).toInt() + count;
            groupedVehicles[index] = existing;
        } else {
            vehicle["Count"] = count;
            groupedIndex.insert(key, groupedVehicles.size());
            groupedVehicles.append(vehicle);
        }
    }

    for (const QVariant &entry : groupedVehicles) {
        const QVariantMap vehicle = entry.toMap();
        xml.writeStartElement(QStringLiteral("Vehicle"));
        const QStringList fields = {
            QStringLiteral("ModuleConfigDir"), QStringLiteral("ModuleConfig"),
            QStringLiteral("IsOrientationForward"), QStringLiteral("PayloadCoeff"),
            QStringLiteral("Count"), QStringLiteral("IsBrakeShoes")
        };
        for (const QString &field : fields)
            xml.writeTextElement(field, vehicle.value(field).toString());
        xml.writeEndElement(); // Vehicle
    }

    xml.writeEndElement(); // Config
    xml.writeEndDocument();
    if (!file.commit()) {
        if (errorMessage) *errorMessage = file.errorString().isEmpty()
            ? QStringLiteral("Ошибка формирования XML-файла.") : file.errorString();
        return false;
    }
    return true;
}

// Принимаем только XML конфигурации подвижной единицы: корневой Config
// должен содержать непосредственные дочерние Vehicle и Model/ModelName.
static bool isVehicleConfigFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QXmlStreamReader xml(&file);
    int depth = 0;
    bool validRoot = false;
    bool hasVehicle = false;
    bool hasMainModel = false;
    bool insideTopLevelModel = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (depth == 0 && xml.name() == QStringLiteral("Config")) validRoot = true;
            if (depth == 1 && xml.name() == QStringLiteral("Vehicle")) hasVehicle = true;
            if (depth == 1 && xml.name() == QStringLiteral("Model")) insideTopLevelModel = true;
            else if (insideTopLevelModel && depth == 2 && xml.name() == QStringLiteral("ModelName")) {
                if (!xml.readElementText().trimmed().isEmpty()) hasMainModel = true;
                continue;
            }
            ++depth;
        } else if (xml.isEndElement()) {
            --depth;
            if (depth == 1 && xml.name() == QStringLiteral("Model")) insideTopLevelModel = false;
        }
    }
    return !xml.hasError() && validRoot && hasVehicle && hasMainModel;
}



// Возвращает ModelName из первого непосредственного блока Model внутри Config.
// Путь из XML всегда относительный к <корень симулятора>/data/models.
static QString readPrimaryModelName(const QString &configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString text = QString::fromUtf8(file.readAll());
    // Убираем XML-комментарии, чтобы закомментированные Model не участвовали в поиске.
    text.remove(QRegularExpression(QStringLiteral("<!--[\\s\\S]*?-->")));

    // Берём только первый активный блок Model верхнего уровня.
    const QRegularExpression modelBlock(
        QStringLiteral("<Model\\s*>([\\s\\S]*?)</Model\\s*>"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch modelMatch = modelBlock.match(text);
    if (!modelMatch.hasMatch())
        return {};

    const QRegularExpression modelNameTag(
        QStringLiteral("<ModelName\\s*>([\\s\\S]*?)</ModelName\\s*>"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch nameMatch = modelNameTag.match(modelMatch.captured(1));
    if (!nameMatch.hasMatch())
        return {};

    QString value = nameMatch.captured(1).trimmed();
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    value.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    return value;
}

class GltfPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    struct Vertex { float x, y, z, u, v; };
    struct DrawBatch { QVector<Vertex> data; int texture = -1; int emissiveTexture = -1; };
    struct CachedModel {
        QVector<DrawBatch> batches;
        QVector<QImage> textureImages;
        QVector3D center;
        float radius = 10.0f;
        QString status;
    };
    static QHash<QString, CachedModel> &modelCache() {
        static QHash<QString, CachedModel> cache;
        return cache;
    }
public:
    // Готовые миниатюры разделяются между карточками состава.
    static QHash<QString, QPixmap> &thumbnailCache() {
        static QHash<QString, QPixmap> cache;
        return cache;
    }
    static QHash<QString, QList<QPointer<QLabel>>> &pendingThumbnails() {
        static QHash<QString, QList<QPointer<QLabel>>> pending;
        return pending;
    }

    explicit GltfPreviewWidget(QWidget *parent=nullptr) : QOpenGLWidget(parent) {
        setMinimumHeight(190); setFocusPolicy(Qt::StrongFocus);
        setStyleSheet("background:#1f252d; border:1px solid #394757; border-radius:6px;");
    }
    void setThumbnailMode() {
        // Карточка состава показывает фиксированный боковой ракурс без управления мышью.
        setFixedSize(132, 88);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        yaw = 90.0f;
        pitch = 0.0f;
        zoom = 1.15f;
        pan = QVector2D();
    }
    void setStatusText(const QString &text) { status = text; update(); }

    void loadModel(const QString &path, const QString &ktxToolPath = QString()) {
        batches.clear(); textureImages.clear(); modelLabel=QFileInfo(path).fileName();
        if (path.isEmpty()) { status="В конфиге не найдена основная модель"; update(); return; }

        // Кэш CPU-геометрии и декодированных текстур общий для всех окон и миниатюр.
        // Учитываем время изменения GLTF, чтобы после обновления модели кэш обновился.
        const QFileInfo modelInfo(path);
        const QString cacheKey = modelInfo.canonicalFilePath() + "|" +
            QString::number(modelInfo.lastModified().toMSecsSinceEpoch()) + "|" + ktxToolPath;
        auto &cache = modelCache();
        const auto cached = cache.constFind(cacheKey);
        if (cached != cache.constEnd()) {
            batches = cached->batches;
            textureImages = cached->textureImages;
            center = cached->center;
            radius = cached->radius;
            status = cached->status + " · кэш";
            uploadedTextures = false;
            textureIds.clear();
            update();
            return;
        }

        QFile f(path);
        if(!f.open(QIODevice::ReadOnly)){ status=QString("Не удалось открыть GLTF: %1").arg(path); update(); return; }
        QJsonParseError parseError{};
        QJsonDocument doc=QJsonDocument::fromJson(f.readAll(), &parseError);
        if(!doc.isObject()){status=QString("Некорректный GLTF: %1").arg(parseError.errorString());update();return;}
        QJsonObject root=doc.object();
        QJsonArray bufs=root.value("buffers").toArray();
        if(bufs.isEmpty()){status="В модели нет BIN-буфера";update();return;}
        QString uri=bufs.first().toObject().value("uri").toString();
        uri=QDir::fromNativeSeparators(uri);
        QFile bf(QFileInfo(path).dir().filePath(uri));
        if(!bf.open(QIODevice::ReadOnly)){status=QString("Не найден BIN: %1").arg(bf.fileName());update();return;}
        QByteArray bin=bf.readAll();

        // Decode KTX2 files into a private temporary directory. Source simulator files stay read-only.
        QJsonArray images=root.value("images").toArray();
        QJsonArray textures=root.value("textures").toArray();
        textureImages.resize(images.size());
        if (!ktxToolPath.isEmpty() && QFileInfo::exists(ktxToolPath)) {
            for (int i=0;i<images.size();++i) {
                QString imageUri=images[i].toObject().value("uri").toString();
                imageUri=QDir::fromNativeSeparators(imageUri);
                const QString source=QFileInfo(path).dir().filePath(imageUri);
                if (!QFileInfo::exists(source)) continue;
                const QString output=textureTemp.path()+QString("/texture_%1.png").arg(i);
                QProcess proc;
                proc.start(ktxToolPath, {"extract","--transcode","rgba8",source,output});
                if (proc.waitForStarted(3000) && proc.waitForFinished(30000) &&
                    proc.exitStatus()==QProcess::NormalExit && proc.exitCode()==0) {
                    QImage img(output);
                    if (!img.isNull()) textureImages[i]=img.convertToFormat(QImage::Format_RGBA8888);
                }
            }
        }

        QJsonArray views=root.value("bufferViews").toArray(), acc=root.value("accessors").toArray();
        QJsonArray meshes=root.value("meshes").toArray(), materials=root.value("materials").toArray();
        QJsonArray nodes=root.value("nodes").toArray();
        QVector<QPair<int,QMatrix4x4>> sceneMeshes;
        std::function<void(int,const QMatrix4x4&)> visitNode = [&](int nodeIndex, const QMatrix4x4 &parent) {
            if(nodeIndex<0 || nodeIndex>=nodes.size()) return;
            const QJsonObject node=nodes[nodeIndex].toObject();
            QMatrix4x4 local;
            if(node.contains("matrix")) {
                const QJsonArray matrix=node.value("matrix").toArray();
                if(matrix.size()==16) for(int col=0;col<4;++col) for(int row=0;row<4;++row)
                    local(row,col)=float(matrix[col*4+row].toDouble());
            } else {
                const QJsonArray t=node.value("translation").toArray();
                if(t.size()==3) local.translate(float(t[0].toDouble()),float(t[1].toDouble()),float(t[2].toDouble()));
                const QJsonArray r=node.value("rotation").toArray();
                if(r.size()==4) local.rotate(QQuaternion(float(r[3].toDouble()),float(r[0].toDouble()),
                                                         float(r[1].toDouble()),float(r[2].toDouble())));
                const QJsonArray sc=node.value("scale").toArray();
                if(sc.size()==3) local.scale(float(sc[0].toDouble()),float(sc[1].toDouble()),float(sc[2].toDouble()));
            }
            const QMatrix4x4 world=parent*local;
            const int meshIndex=node.value("mesh").toInt(-1);
            if(meshIndex>=0 && meshIndex<meshes.size()) sceneMeshes.append(qMakePair(meshIndex,world));
            for(const auto &child:node.value("children").toArray()) visitNode(child.toInt(-1),world);
        };
        const QJsonArray scenes=root.value("scenes").toArray();
        const int sceneIndex=root.value("scene").toInt(0);
        if(sceneIndex>=0 && sceneIndex<scenes.size()) {
            for(const auto &rootNode:scenes[sceneIndex].toObject().value("nodes").toArray()) {
                QMatrix4x4 identity; visitNode(rootNode.toInt(-1),identity);
            }
        } else {
            for(int ni=0;ni<nodes.size();++ni) if(nodes[ni].toObject().contains("mesh")) {
                QMatrix4x4 identity; visitNode(ni,identity);
            }
        }
        for(const auto &sceneMesh:sceneMeshes) {
            const QJsonObject meshValue=meshes[sceneMesh.first].toObject();
            const QMatrix4x4 nodeTransform=sceneMesh.second;
            for(const auto &primitiveValue:meshValue.value("primitives").toArray()) {
                QJsonObject prim=primitiveValue.toObject();
                QJsonObject attrs=prim.value("attributes").toObject();
                int posIndex=attrs.value("POSITION").toInt(-1);
                if(posIndex<0 || posIndex>=acc.size()) continue;
                auto readAccessor=[&](int accessorIndex, int components, QVector<float> &out)->bool {
                    if(accessorIndex<0 || accessorIndex>=acc.size()) return false;
                    QJsonObject a=acc[accessorIndex].toObject();
                    int viewIndex=a.value("bufferView").toInt(-1);
                    if(viewIndex<0 || viewIndex>=views.size()) return false;
                    QJsonObject view=views[viewIndex].toObject();
                    int componentType=a.value("componentType").toInt();
                    int componentBytes=(componentType==5126 || componentType==5125)?4:
                                       (componentType==5123?2:(componentType==5121?1:0));
                    if(componentBytes==0) return false;
                    int count=a.value("count").toInt();
                    int stride=view.value("byteStride").toInt(components*componentBytes);
                    int offset=view.value("byteOffset").toInt()+a.value("byteOffset").toInt();
                    out.resize(count*components);
                    for(int n=0;n<count;++n) for(int c=0;c<components;++c) {
                        int at=offset+n*stride+c*componentBytes;
                        if(at+componentBytes>bin.size()) return false;
                        float value=0;
                        if(componentType==5126) memcpy(&value,bin.constData()+at,4);
                        else if(componentType==5123){quint16 q;memcpy(&q,bin.constData()+at,2);value=q;}
                        else if(componentType==5121) value=(uchar)bin[at];
                        else if(componentType==5125){quint32 q;memcpy(&q,bin.constData()+at,4);value=float(q);}
                        out[n*components+c]=value;
                    }
                    return true;
                };
                QVector<float> positions, uvs;
                if(!readAccessor(posIndex,3,positions)) continue;
                const int uvIndex=attrs.value("TEXCOORD_0").toInt(-1);
                const bool hasUv=readAccessor(uvIndex,2,uvs) && uvs.size()/2==positions.size()/3;
                QVector<quint32> indices;
                if(prim.contains("indices")) {
                    int indexAccessor=prim.value("indices").toInt(-1);
                    if(indexAccessor<0 || indexAccessor>=acc.size()) continue;
                    QJsonObject ia=acc[indexAccessor].toObject();
                    int vi=ia.value("bufferView").toInt(-1);
                    if(vi<0 || vi>=views.size()) continue;
                    QJsonObject vw=views[vi].toObject();
                    int ct=ia.value("componentType").toInt();
                    int bytes=ct==5125?4:(ct==5123?2:(ct==5121?1:0));
                    if(!bytes) continue;
                    int off=vw.value("byteOffset").toInt()+ia.value("byteOffset").toInt();
                    int count=ia.value("count").toInt();
                    for(int n=0;n<count;++n) {
                        int at=off+n*bytes; if(at+bytes>bin.size()) break;
                        quint32 q=0;
                        if(bytes==4) memcpy(&q,bin.constData()+at,4);
                        else if(bytes==2){quint16 t;memcpy(&t,bin.constData()+at,2);q=t;}
                        else q=(uchar)bin[at];
                        indices.append(q);
                    }
                } else {
                    for(int n=0;n<positions.size()/3;++n) indices.append(quint32(n));
                }
                DrawBatch batch;
                int materialIndex=prim.value("material").toInt(-1);
                if(materialIndex>=0 && materialIndex<materials.size()) {
                    const QJsonObject material=materials[materialIndex].toObject();
                    const QJsonObject pbr=material.value("pbrMetallicRoughness").toObject();
                    const int baseTextureIndex=pbr.value("baseColorTexture").toObject().value("index").toInt(-1);
                    if(baseTextureIndex>=0 && baseTextureIndex<textures.size())
                        batch.texture=textures[baseTextureIndex].toObject().value("source").toInt(-1);
                    const int emissiveTextureIndex=material.value("emissiveTexture").toObject().value("index").toInt(-1);
                    if(emissiveTextureIndex>=0 && emissiveTextureIndex<textures.size())
                        batch.emissiveTexture=textures[emissiveTextureIndex].toObject().value("source").toInt(-1);
                }
                for(quint32 idx:indices) {
                    int p=int(idx)*3;
                    if(p+2>=positions.size()) continue;
                    const QVector3D transformed=nodeTransform.map(QVector3D(positions[p],positions[p+1],positions[p+2]));
                    Vertex v{transformed.x(),transformed.y(),transformed.z(),0,0};
                    if(hasUv){v.u=uvs[int(idx)*2];v.v=uvs[int(idx)*2+1];}
                    batch.data.append(v);
                }
                if(!batch.data.isEmpty()) batches.append(batch);
            }
        }
        QVector3D mn(1e30f,1e30f,1e30f), mx(-1e30f,-1e30f,-1e30f);
        int vertexCount=0;
        for(const auto &batch:batches) for(const auto &v:batch.data) {
            mn.setX(qMin(mn.x(),v.x)); mn.setY(qMin(mn.y(),v.y)); mn.setZ(qMin(mn.z(),v.z));
            mx.setX(qMax(mx.x(),v.x)); mx.setY(qMax(mx.y(),v.y)); mx.setZ(qMax(mx.z(),v.z)); ++vertexCount;
        }
        if(!vertexCount){status="Геометрия не найдена";update();return;}
        center=(mn+mx)*0.5f; radius=qMax(1.0f,(mx-mn).length()*0.5f);
        uploadedTextures=false;
        int textured=0; for(const auto &img:textureImages) if(!img.isNull()) ++textured;
        int texturedBatches=0; for(const auto &batch:batches) if(batch.texture>=0) ++texturedBatches;
        status=QString("Геометрия загружена · узлов: %1 · изображений: %2/%3 · материалов с текстурой: %4")
                   .arg(sceneMeshes.size()).arg(textured).arg(textureImages.size()).arg(texturedBatches);

        CachedModel cachedModel;
        cachedModel.batches = batches;
        cachedModel.textureImages = textureImages;
        cachedModel.center = center;
        cachedModel.radius = radius;
        cachedModel.status = status;
        // Ограничиваем размер кэша, чтобы большие составы не удерживали память бесконечно.
        if (cache.size() >= 24)
            cache.erase(cache.begin());
        cache.insert(cacheKey, cachedModel);
        update();
    }
protected:
    void initializeGL() override { initializeOpenGLFunctions(); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); }
    void resizeGL(int w,int h) override { glViewport(0,0,w,h); }
    void paintGL() override {
        glClearColor(0.10f,0.13f,0.17f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(batches.isEmpty()){QPainter p(this);p.setPen(QColor("#b8c9df"));p.drawText(rect(),Qt::AlignCenter,status.isEmpty()?"Выберите подвижную единицу":status);return;}
        if(!program){
            program=new QOpenGLShaderProgram;
            program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                "attribute highp vec3 pos; attribute highp vec2 texCoord; uniform mat4 mvp; varying highp vec2 uv; varying highp vec3 n; void main(){gl_Position=mvp*vec4(pos,1.0);uv=texCoord;n=normalize(pos);}");
            program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                "uniform sampler2D diffuseTex; uniform sampler2D emissiveTex; uniform int useTexture; uniform int useEmissive; varying highp vec2 uv; varying highp vec3 n; void main(){float l=0.58+0.42*abs(dot(normalize(n),normalize(vec3(0.3,0.8,0.5)))); vec4 c=useTexture==1?texture2D(diffuseTex,uv):vec4(0.62,0.70,0.78,1.0); vec3 e=useEmissive==1?texture2D(emissiveTex,uv).rgb:vec3(0.0); gl_FragColor=vec4(c.rgb*l+e,c.a);}");
            program->link();
        }
        if(!uploadedTextures) {
            textureIds.fill(0,textureImages.size());
            for(int i=0;i<textureImages.size();++i) if(!textureImages[i].isNull()) {
                GLuint id=0; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
                QImage img=textureImages[i].convertToFormat(QImage::Format_RGBA8888);
                glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,img.width(),img.height(),0,GL_RGBA,GL_UNSIGNED_BYTE,img.constBits());
                glGenerateMipmap(GL_TEXTURE_2D); textureIds[i]=id;
            }
            uploadedTextures=true;
        }
        QMatrix4x4 proj,view,model;
        proj.perspective(35.0f,float(width())/qMax(1,height()),0.01f,10000.0f);
        view.translate(pan.x(),pan.y(),-radius*2.8f/zoom);
        model.translate(-center); model.rotate(yaw,0,1,0); model.rotate(pitch,1,0,0);
        QMatrix4x4 mvp=proj*view*model;
        program->bind(); program->setUniformValue("mvp",mvp);
        program->setUniformValue("diffuseTex",0); program->setUniformValue("emissiveTex",1);
        int posLoc=program->attributeLocation("pos"), uvLoc=program->attributeLocation("texCoord");
        for(const auto &batch:batches) {
            GLuint tex=(batch.texture>=0 && batch.texture<textureIds.size())?textureIds[batch.texture]:0;
            program->setUniformValue("useTexture",tex?1:0);
            if(tex){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);}
            const GLuint emissive=(batch.emissiveTexture>=0 && batch.emissiveTexture<textureIds.size())?textureIds[batch.emissiveTexture]:0;
            program->setUniformValue("useEmissive",emissive?1:0);
            if(emissive){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,emissive);}
            if(!vertexBuffer) glGenBuffers(1,&vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(batch.data.size()*sizeof(Vertex)),
                         batch.data.constData(),GL_STATIC_DRAW);
            program->enableAttributeArray(posLoc);
            glVertexAttribPointer(GLuint(posLoc),3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(0));
            program->enableAttributeArray(uvLoc);
            glVertexAttribPointer(GLuint(uvLoc),2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(3*sizeof(float)));
            glDrawArrays(GL_TRIANGLES,0,batch.data.size());
            program->disableAttributeArray(posLoc); program->disableAttributeArray(uvLoc);
            glBindBuffer(GL_ARRAY_BUFFER,0);
            glActiveTexture(GL_TEXTURE0);
        }
        program->release();
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::MiddleButton) {
            // Нажатие колеса возвращает камеру к центру модели и исходному ракурсу.
            yaw = 90.0f;
            pitch = 0.0f;
            zoom = 1.15f;
            pan = QVector2D();
            update();
            e->accept();
            return;
        }
        last = e->pos();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        const QPoint d = e->pos() - last;
        last = e->pos();
        if (e->buttons() & Qt::LeftButton) {
            // Инвертированы обе оси вращения камеры.
            yaw -= d.x() * 0.6f;
            pitch = qBound(-89.0f, pitch - d.y() * 0.5f, 89.0f);
        } else if (e->buttons() & Qt::RightButton) {
            pan += QVector2D(d.x(), -d.y()) * (radius * 0.002f / zoom);
        }
        update();
    }
    void wheelEvent(QWheelEvent *e) override {zoom=qBound(0.2f,zoom*(e->angleDelta().y()>0?1.15f:0.87f),8.0f);update();}
    void mouseDoubleClickEvent(QMouseEvent *) override {yaw=90;pitch=0;zoom=1.15f;pan={};update();}
private:
    QVector<DrawBatch> batches;
    QVector<QImage> textureImages;
    QVector<GLuint> textureIds;
    QVector3D center; QVector2D pan;
    float radius=10,zoom=1.15f,yaw=90,pitch=0;
    QPoint last; QString status,modelLabel;
    QOpenGLShaderProgram *program=nullptr;
    GLuint vertexBuffer=0;
    QTemporaryDir textureTemp;
    bool uploadedTextures=false;
};

// Получение физических графических адаптеров Windows через DXGI.
static QStringList detectGraphicsAdapters()
{
    QStringList adapters;
#ifdef Q_OS_WIN
    IDXGIFactory1 *factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                     reinterpret_cast<void **>(&factory)))) {
        for (UINT index = 0; ; ++index) {
            IDXGIAdapter1 *adapter = nullptr;
            if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;
            if (!adapter)
                continue;

            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) &&
                !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                const QString name = QString::fromWCharArray(desc.Description).trimmed();
                if (!name.isEmpty() && !adapters.contains(name, Qt::CaseInsensitive))
                    adapters.append(name);
            }
            adapter->Release();
        }
        factory->Release();
    }
#endif
    return adapters;
}

// Единый выпадающий список для всех подразделов настроек.
// Стрелку отрисовывает стандартный стиль Qt — так она одинаковая
// на страницах «Главное», «Графика» и «Сеть».
class GraphicsComboBox : public QComboBox
{
public:
    explicit GraphicsComboBox(QWidget *parent = nullptr) : QComboBox(parent)
    {
        setFixedWidth(500);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);
        // Единая стрелка для всех выпадающих списков настроек.
        // Нативную стрелку скрываем в stylesheet, чтобы не было наложения.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        QPen pen(arrowColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        const qreal cx = width() - 15.0;
        const qreal cy = height() / 2.0;
        QPainterPath chevron;
        chevron.moveTo(cx - 4.0, cy - 1.5);
        chevron.lineTo(cx, cy + 2.5);
        chevron.lineTo(cx + 4.0, cy - 1.5);
        painter.drawPath(chevron);
    }
};

// Специальный вариант только для выпадающих списков на странице «Главное».
// Рисует тот же шеврон, что GraphicsComboBox, но исключает системную стрелку
// из комплексной отрисовки Qt, чтобы две стрелки не накладывались друг на друга.
class MainSettingsComboBox : public GraphicsComboBox
{
public:
    explicit MainSettingsComboBox(QWidget *parent = nullptr) : GraphicsComboBox(parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        QStyleOptionComboBox option;
        initStyleOption(&option);
        option.subControls &= ~QStyle::SC_ComboBoxArrow;
        painter.drawComplexControl(QStyle::CC_ComboBox, option);
        painter.drawControl(QStyle::CE_ComboBoxLabel, option);

        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setPen(QPen(arrowColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal cx = width() - 15.0;
        const qreal cy = height() / 2.0;
        QPainterPath chevron;
        chevron.moveTo(cx - 4.0, cy - 1.5);
        chevron.lineTo(cx, cy + 2.5);
        chevron.lineTo(cx + 4.0, cy - 1.5);
        painter.drawPath(chevron);
    }
};

// Кнопка выбора журнала: рисуем стрелку тем же шевроном, что и в настройках.
// Системный menu-indicator скрыт, чтобы стрелки не дублировались.
class JournalMenuButton : public QToolButton
{
public:
    explicit JournalMenuButton(QWidget *parent = nullptr) : QToolButton(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QToolButton::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setPen(QPen(arrowColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal cx = width() - 15.0;
        const qreal cy = height() / 2.0;
        QPainterPath chevron;
        chevron.moveTo(cx - 4.0, cy - 1.5);
        chevron.lineTo(cx, cy + 2.5);
        chevron.lineTo(cx + 4.0, cy - 1.5);
        painter.drawPath(chevron);
    }
};

class GraphicsCheckBox : public QCheckBox
{
public:
    explicit GraphicsCheckBox(const QString &text, QWidget *parent = nullptr)
        : QCheckBox(text, parent) {}

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QCheckBox::paintEvent(event);
        if (!isChecked())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor("#63bdff"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);

        // Галочка поверх индикатора: не заменяем её синей заливкой.
        const qreal cy = (height() - 16.0) / 2.0;
        QPainterPath mark;
        mark.moveTo(4.0, cy + 8.0);
        mark.lineTo(7.0, cy + 11.0);
        mark.lineTo(13.0, cy + 4.5);
        painter.drawPath(mark);
    }
};

class GraphicsSpinBox : public QSpinBox
{
public:
    explicit GraphicsSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) { setFixedWidth(500); }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        // Обрабатываем правую область самостоятельно, чтобы верхняя и нижняя
        // стрелки всегда вызывали разные действия независимо от стиля Qt.
        const int arrowAreaWidth = 42;
        if (event->button() == Qt::LeftButton &&
            event->position().x() >= width() - arrowAreaWidth) {
            if (event->position().y() < height() / 2.0)
                stepUp();
            else
                stepDown();
            event->accept();
            return;
        }
        QSpinBox::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QSpinBox::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setBrush(arrowColor);
        const qreal cx = width() - 14.0;
        const qreal cy = height() / 2.0;
        // Разносим стрелки дальше друг от друга по вертикали,
        // сохраняя отдельные зоны нажатия для увеличения и уменьшения.
        QPolygonF up;
        up << QPointF(cx - 4, cy - 5) << QPointF(cx + 4, cy - 5) << QPointF(cx, cy - 10);
        QPolygonF down;
        down << QPointF(cx - 4, cy + 5) << QPointF(cx + 4, cy + 5) << QPointF(cx, cy + 10);
        painter.drawPolygon(up);
        painter.drawPolygon(down);
    }
};


class GraphicsDoubleSpinBox : public QDoubleSpinBox
{
public:
    explicit GraphicsDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent) {
        // Повторяем подход GraphicsSpinBox из настроек: штатные кнопки Qt
        // остаются на месте, а стрелки поверх них рисуются тем же способом.
        setFixedWidth(650);
        setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        setAccelerated(false);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setKeyboardTracking(true);
        setFrame(true);
        setStyleSheet(
            "QDoubleSpinBox { padding:5px 34px 5px 8px; }"
            "QDoubleSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:30px; height:50%; margin:0; border:0; background:transparent; border-top-right-radius:5px; }"
            "QDoubleSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:30px; height:50%; margin:0; border:0; background:transparent; border-bottom-right-radius:5px; }"
            "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover, QDoubleSpinBox::up-button:pressed, QDoubleSpinBox::down-button:pressed { background:transparent; border:0; }"
        );
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        const int arrowAreaWidth = 42;
        if (event->button() == Qt::LeftButton && event->position().x() >= width() - arrowAreaWidth) {
            if (event->position().y() < height() / 2.0) stepUp(); else stepDown();
            event->accept();
            return;
        }
        QDoubleSpinBox::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent *event) override {
        QDoubleSpinBox::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setBrush(arrowColor);
        const qreal cx = width() - 14.0, cy = height() / 2.0;
        QPolygonF up; up << QPointF(cx - 4, cy - 5) << QPointF(cx + 4, cy - 5) << QPointF(cx, cy - 10);
        QPolygonF down; down << QPointF(cx - 4, cy + 5) << QPointF(cx + 4, cy + 5) << QPointF(cx, cy + 10);
        painter.drawPolygon(up);
        painter.drawPolygon(down);
    }
};

class GraphicsTimeEdit : public QTimeEdit
{
public:
    explicit GraphicsTimeEdit(const QTime &time, QWidget *parent = nullptr) : QTimeEdit(time, parent) {
        // Та же реализация, что у GraphicsSpinBox в настройках.
        setFixedWidth(140);
        setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        setAccelerated(false);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setFrame(true);
        setStyleSheet(
            "QTimeEdit { padding:5px 34px 5px 8px; }"
            "QTimeEdit::up-button { subcontrol-origin:border; subcontrol-position:top right; width:30px; height:50%; margin:0; border:0; background:transparent; border-top-right-radius:5px; }"
            "QTimeEdit::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:30px; height:50%; margin:0; border:0; background:transparent; border-bottom-right-radius:5px; }"
            "QTimeEdit::up-button:hover, QTimeEdit::down-button:hover, QTimeEdit::up-button:pressed, QTimeEdit::down-button:pressed { background:transparent; border:0; }"
        );
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        const int arrowAreaWidth = 42;
        if (event->button() == Qt::LeftButton && event->position().x() >= width() - arrowAreaWidth) {
            if (event->position().y() < height() / 2.0) stepUp(); else stepDown();
            event->accept();
            return;
        }
        QTimeEdit::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent *event) override {
        QTimeEdit::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setBrush(arrowColor);
        const qreal cx = width() - 14.0, cy = height() / 2.0;
        QPolygonF up; up << QPointF(cx - 4, cy - 5) << QPointF(cx + 4, cy - 5) << QPointF(cx, cy - 10);
        QPolygonF down; down << QPointF(cx - 4, cy + 5) << QPointF(cx + 4, cy + 5) << QPointF(cx, cy + 10);
        painter.drawPolygon(up);
        painter.drawPolygon(down);
    }
};

class GraphicsDateEdit : public QDateEdit
{
public:
    explicit GraphicsDateEdit(const QDate &date, QWidget *parent = nullptr) : QDateEdit(date, parent) { setFixedWidth(500); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        QStyleOptionSpinBox option;
        initStyleOption(&option);
        option.subControls &= ~QStyle::SC_SpinBoxUp;
        option.subControls &= ~QStyle::SC_SpinBoxDown;
        painter.drawComplexControl(QStyle::CC_SpinBox, option);

        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor arrowColor = palette().color(QPalette::ButtonText);
        if (!arrowColor.isValid()) arrowColor = palette().color(QPalette::Text);
        painter.setPen(QPen(arrowColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal cx = width() - 15.0;
        const qreal cy = height() / 2.0;
        QPainterPath chevron;
        chevron.moveTo(cx - 4.0, cy - 1.5);
        chevron.lineTo(cx, cy + 2.5);
        chevron.lineTo(cx + 4.0, cy - 1.5);
        painter.drawPath(chevron);
    }
};

static QLabel *heading(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(15);
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet("background: transparent; border: none; padding: 0px;");
    return label;
}

static QFrame *makePanel()
{
    auto *panel = new QFrame;
    panel->setObjectName("panel");
    return panel;
}

static QIcon makeMenuIcon(int index)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#9bbce5"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (index) {
    case 0: // Основное — контурная панель с ползунками, отличается от значка «Главная»
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2);
        p.drawLine(QPointF(7, 9), QPointF(17, 9));
        p.drawLine(QPointF(7, 15), QPointF(17, 15));
        p.drawEllipse(QRectF(10, 7, 4, 4));
        p.drawEllipse(QRectF(14, 13, 4, 4));
        break;
    case 1:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawEllipse(QRectF(8, 3, 8, 18));
        p.drawLine(QPointF(4, 9), QPointF(20, 9));
        p.drawLine(QPointF(4, 15), QPointF(20, 15));
        break;
    case 2:
        p.drawLine(QPointF(12, 3), QPointF(12, 15));
        p.drawLine(QPointF(7.5, 10.5), QPointF(12, 15));
        p.drawLine(QPointF(16.5, 10.5), QPointF(12, 15));
        p.drawRoundedRect(QRectF(4, 16, 16, 5), 1.5, 1.5);
        p.drawLine(QPointF(8, 18.5), QPointF(16, 18.5));
        break;
    case 3: // Скриншоты — контурная камера
        p.drawRoundedRect(QRectF(3, 7, 18, 13), 2, 2);
        p.drawRoundedRect(QRectF(7, 4, 7, 3), 1, 1);
        p.drawEllipse(QRectF(9, 10, 6, 6));
        p.drawEllipse(QRectF(10.5, 11.5, 3, 3));
        break;
    case 4:
        p.drawEllipse(QRectF(8, 8, 8, 8));
        for (int a = 0; a < 8; ++a) {
            const qreal angle = a * 3.14159265358979323846 / 4.0;
            const QPointF inner(12 + 5.5 * qCos(angle), 12 + 5.5 * qSin(angle));
            const QPointF outer(12 + 8.5 * qCos(angle), 12 + 8.5 * qSin(angle));
            p.drawLine(inner, outer);
        }
        break;
    case 5: // Поезда
        p.drawRoundedRect(QRectF(3, 7, 14, 9), 2, 2);
        p.drawRect(QRectF(17, 10, 4, 6));
        p.drawEllipse(QRectF(5, 15, 4, 4));
        p.drawEllipse(QRectF(15, 15, 4, 4));
        break;
    case 6: // Дополнения
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 3, 3);
        p.drawLine(QPointF(12, 4), QPointF(12, 9));
        p.drawLine(QPointF(4, 12), QPointF(9, 12));
        p.drawLine(QPointF(15, 12), QPointF(20, 12));
        break;
    case 7: // Журнал
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 2, 2);
        p.drawLine(QPointF(8, 8), QPointF(16, 8));
        p.drawLine(QPointF(8, 12), QPointF(16, 12));
        p.drawLine(QPointF(8, 16), QPointF(14, 16));
        break;
    }
    p.end();
    return QIcon(pixmap);
}


static QIcon makeHomeTabIcon(int index)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#9bbce5"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (index) {
    case 0: // Сценарии — лист с маршрутной линией
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 2, 2);
        p.drawLine(QPointF(8, 7), QPointF(16, 7));
        p.drawLine(QPointF(8, 11), QPointF(13, 11));
        p.drawEllipse(QRectF(8, 14, 3, 3));
        p.drawLine(QPointF(11, 15.5), QPointF(16, 15.5));
        p.drawEllipse(QRectF(15, 14, 3, 3));
        break;
    case 1: // Поезда — локомотив
        p.drawRoundedRect(QRectF(3, 8, 14, 8), 1.5, 1.5);
        p.drawRect(QRectF(17, 11, 4, 5));
        p.drawLine(QPointF(6, 8), QPointF(6, 5));
        p.drawLine(QPointF(6, 5), QPointF(12, 5));
        p.drawLine(QPointF(12, 5), QPointF(14, 8));
        p.drawLine(QPointF(5, 18), QPointF(20, 18));
        p.drawEllipse(QRectF(6, 14, 4, 4));
        p.drawEllipse(QRectF(15, 14, 4, 4));
        p.drawLine(QPointF(9, 11), QPointF(13, 11));
        break;
    case 2: // Дополнения — модуль/пазл
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawLine(QPointF(12, 4), QPointF(12, 9));
        p.drawLine(QPointF(4, 12), QPointF(9, 12));
        p.drawLine(QPointF(15, 12), QPointF(20, 12));
        p.drawLine(QPointF(12, 15), QPointF(12, 20));
        p.drawEllipse(QRectF(9.5, 9.5, 5, 5));
        break;
    case 3: // Журнал — документ с текстовыми строками
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 2, 2);
        p.drawLine(QPointF(8, 8), QPointF(16, 8));
        p.drawLine(QPointF(8, 12), QPointF(16, 12));
        p.drawLine(QPointF(8, 16), QPointF(14, 16));
        break;
    }
    p.end();
    return QIcon(pixmap);
}

static QIcon makeSettingsIcon(int index)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#9bbce5"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (index) {
    case 0:
        p.drawRoundedRect(QRectF(3.5, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13.5, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(3.5, 14, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13.5, 14, 7, 7), 1.5, 1.5);
        break;
    case 1:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawLine(QPointF(12, 4), QPointF(12, 20));
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        break;
    case 2:
        p.drawEllipse(QRectF(4, 4, 16, 16));
        p.drawLine(QPointF(7, 17), QPointF(17, 7));
        break;
    case 3:
        p.drawRoundedRect(QRectF(3, 4, 18, 13), 2, 2);
        p.drawLine(QPointF(12, 17), QPointF(12, 21));
        p.drawLine(QPointF(8, 21), QPointF(16, 21));
        break;
    case 4:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawEllipse(QRectF(8, 3, 8, 18));
        p.drawLine(QPointF(4, 9), QPointF(20, 9));
        p.drawLine(QPointF(4, 15), QPointF(20, 15));
        break;
    }
    p.end();
    return QIcon(pixmap);
}

class ResponsiveConsistListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

    void updateCardWidths()
    {
        // Карточки остаются фиксированной ширины и идут цепочкой слева направо.
        // Вертикально центрируем ряд карточек в доступной высоте ленты.
        constexpr int cardHeight = 148;
        const int verticalOffset = qMax(0, (height() - cardHeight) / 2);
        setGridSize(QSize(154, cardHeight));
        setViewportMargins(0, verticalOffset, 0, 0);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QListWidget::resizeEvent(event);
        updateCardWidths();
    }
};

// Чтение версии продукта из ресурсов версии simulator.exe (Windows).
static QString simulatorProductVersion(const QString &executablePath)
{
#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(executablePath).toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(nativePath.c_str(), &ignored);
    if (size == 0)
        return QStringLiteral("не определена");

    QByteArray data(static_cast<int>(size), '\0');
    if (!GetFileVersionInfoW(nativePath.c_str(), 0, size, data.data()))
        return QStringLiteral("не определена");

    VS_FIXEDFILEINFO *info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &infoSize) ||
        !info || infoSize < sizeof(VS_FIXEDFILEINFO))
        return QStringLiteral("не определена");

    const DWORD ms = info->dwProductVersionMS;
    const DWORD ls = info->dwProductVersionLS;
    return QStringLiteral("%1.%2.%3.%4")
        .arg(HIWORD(ms)).arg(LOWORD(ms)).arg(HIWORD(ls)).arg(LOWORD(ls));
#else
    Q_UNUSED(executablePath);
    return QStringLiteral("не определена");
#endif
}


class ScaledScreenshotLabel : public QLabel
{
public:
    explicit ScaledScreenshotLabel(const QPixmap &pixmap, QWidget *parent = nullptr)
        : QLabel(parent), sourcePixmap(pixmap)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(320, 240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
    }
    void setWheelHandler(std::function<void(int)> handler) { wheelHandler = std::move(handler); }
    void setSourcePixmap(const QPixmap &pixmap)
    {
        sourcePixmap = pixmap;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Window));
        if (sourcePixmap.isNull()) return;
        const QSize scaledSize = sourcePixmap.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - scaledSize.width()) / 2,
                                  (height() - scaledSize.height()) / 2), scaledSize);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target, sourcePixmap);
    }
    void wheelEvent(QWheelEvent *event) override
    {
        if (wheelHandler && event->angleDelta().y() != 0) {
            wheelHandler(event->angleDelta().y() > 0 ? -1 : 1);
            event->accept();
            return;
        }
        QLabel::wheelEvent(event);
    }
private:
    QPixmap sourcePixmap;
    std::function<void(int)> wheelHandler;
};

// Чтение и сохранение пользовательских серверов мультиплеера.
// Контейнер Local (127.0.0.1) является системным: не показываем и никогда не изменяем его.
struct SavedMultiplayerServer { QString name; QString host; QString port; };
static bool isLocalMultiplayerServer(const QDomElement &serverElement)
{
    const QString name = serverElement.firstChildElement(QStringLiteral("Name")).text().trimmed();
    const QString host = serverElement.firstChildElement(QStringLiteral("HostAddr")).text().trimmed();
    return name.compare(QStringLiteral("Local"), Qt::CaseInsensitive) == 0 && host == QStringLiteral("127.0.0.1");
}
static QList<SavedMultiplayerServer> loadSavedMultiplayerServers(const QString &path)
{
    QList<SavedMultiplayerServer> result;
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QDomDocument document;
    if (!document.setContent(&file)) return result;
    const QDomElement root = document.documentElement();
    for (QDomElement element = root.firstChildElement(QStringLiteral("Server"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("Server"))) {
        if (isLocalMultiplayerServer(element)) continue;
        SavedMultiplayerServer server;
        server.name = element.firstChildElement(QStringLiteral("Name")).text().trimmed();
        server.host = element.firstChildElement(QStringLiteral("HostAddr")).text().trimmed();
        server.port = element.firstChildElement(QStringLiteral("port")).text().trimmed();
        if (!server.name.isEmpty() && !server.host.isEmpty() && !server.port.isEmpty()) result.append(server);
    }
    return result;
}
static bool saveMultiplayerServers(const QString &path, const QList<SavedMultiplayerServer> &servers)
{
    QDomDocument document;
    QFile input(path);
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        if (!document.setContent(&input)) return false;
        input.close();
    } else {
        document.appendChild(document.createProcessingInstruction(
            QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
        document.appendChild(document.createElement(QStringLiteral("Config")));
    }

    QDomElement root = document.documentElement();
    if (root.isNull()) {
        root = document.createElement(QStringLiteral("Config"));
        document.appendChild(root);
    }

    // Удаляем только пользовательские контейнеры Server. Local и любые другие
    // элементы/комментарии конфигурации остаются нетронутыми.
    for (QDomElement element = root.firstChildElement(QStringLiteral("Server")); !element.isNull();) {
        QDomElement next = element.nextSiblingElement(QStringLiteral("Server"));
        if (!isLocalMultiplayerServer(element)) root.removeChild(element);
        element = next;
    }

    for (const auto &server : servers) {
        if (server.name.trimmed().isEmpty() || server.host.trimmed().isEmpty() || server.port.trimmed().isEmpty()) continue;
        QDomElement element = document.createElement(QStringLiteral("Server"));
        QDomElement name = document.createElement(QStringLiteral("Name"));
        name.appendChild(document.createTextNode(server.name));
        element.appendChild(name);
        QDomElement host = document.createElement(QStringLiteral("HostAddr"));
        host.appendChild(document.createTextNode(server.host));
        element.appendChild(host);
        QDomElement port = document.createElement(QStringLiteral("port"));
        port.appendChild(document.createTextNode(server.port));
        element.appendChild(port);
        root.appendChild(element);
    }

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream stream(&output);
    stream.setEncoding(QStringConverter::Utf8);
    document.save(stream, 4);
    stream.flush();
    return output.commit();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    LauncherLiveTranslationFilter liveTranslationFilter;
    app.installEventFilter(&liveTranslationFilter);
    app.setWindowIcon(QIcon(":/icons/rrs_launcher.png"));
    app.setApplicationName("RRS Launcher");

    // Единый стиль кнопок действий в редакторах сценариев и поездов.
    const QString scenarioActionButtonStyle = QStringLiteral(R"(
        QPushButton#scenarioActionButton {
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #35465a,stop:1 #293746);
            color:#eaf2fb;
            border:1px solid #607b98;
            border-radius:7px;
            padding:7px 13px;
            font-weight:600;
        }
        QPushButton#scenarioActionButton:hover {
            background:#3b536b;
            border-color:#83b7e5;
        }
        QPushButton#scenarioActionButton:pressed {
            background:#24384c;
        }
        QPushButton#scenarioActionButton:disabled {
            color:#8292a3;
            background:#27313d;
            border-color:#465565;
        }
    )");

    // В режиме разработки путь к симулятору запрашивается при каждом запуске. путь к симулятору запрашивается при каждом запуске.
    QString simulatorRoot;
    while (true) {
        const QString selected = QFileDialog::getExistingDirectory(
            nullptr, "Выберите папку симулятора RRS", QString(), QFileDialog::ShowDirsOnly);
        if (selected.isEmpty()) return 0;
        const QDir rootDir(selected);
        if ((rootDir.exists("cfg/train") || rootDir.exists("cfg/trains")) &&
            (rootDir.exists("bin/launcher.exe") || rootDir.exists("bin/display-player.exe"))) {
            simulatorRoot = rootDir.absolutePath();
            break;
        }
        QMessageBox::warning(nullptr, "Папка симулятора не распознана",
                             "В выбранной папке не найдена структура cfg/trains или исполняемые файлы симулятора.\n"
                             "Выберите корневую папку RRS.");
    }

    app.setStyleSheet(R"(
        QWidget { background:#1b222c; color:#e7edf5; font-size:10pt; }
        QMainWindow, QDialog { background:#1b222c; }
        QFrame#panel, QTabWidget::pane {
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #263241,stop:1 #202a36);
            border:1px solid #465c73; border-radius:9px;
        }
        QFrame#panel { padding:2px; }
        QLabel { background:transparent; color:#e7edf5; }
        QLabel#muted { color:#9fb1c6; }
        QListWidget, QTreeWidget, QTableWidget {
            background:#1d2733; color:#dce6f2; border:1px solid #40546a;
            border-radius:7px; outline:0; padding:3px;
            alternate-background-color:#222e3b;
        }
        QListWidget::item, QTreeWidget::item { padding:7px 9px; border-radius:5px; border-bottom:1px solid rgba(111,139,166,28); }
        QListWidget::item:hover, QTreeWidget::item:hover { background:#2b3b4e; color:#ffffff; }
        QListWidget::item:selected, QTreeWidget::item:selected { background:#245d91; color:#ffffff; border:1px solid #438cc7; }
        QListWidget::item:focus, QPushButton:focus, QToolButton:focus, QTabBar::tab:focus { outline:0; }
        QPushButton, QToolButton {
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #35465a,stop:1 #293746);
            color:#eaf2fb; border:1px solid #607b98; border-radius:7px;
            padding:8px 14px; min-height:18px;
        }
        QPushButton:hover, QToolButton:hover { background:#3b5874; border-color:#83b5df; }
        QPushButton:pressed, QToolButton:pressed { background:#24496b; border-color:#6aa8d9; }
        QPushButton:disabled { color:#738397; background:#28313b; border-color:#39495a; }
        QPushButton#primary { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2589d5,stop:1 #1764a7); border-color:#4da5e8; color:white; font-weight:bold; }
        QPushButton#primary:hover { background:#329be8; border-color:#8bcaff; }
        QPushButton#primary:pressed { background:#15568e; }
        QPushButton#navButton { background:transparent; border:1px solid transparent; border-radius:7px; padding:9px 15px; min-height:18px; font-weight:600; }
        QPushButton#navButton:hover { background:#2b3b4e; border:1px solid #4b6680; padding:9px 15px; font-weight:600; }
        QPushButton#navButton:pressed { background:#24496b; border:1px solid #6aa8d9; padding:9px 15px; font-weight:600; }
        QPushButton#navButton:checked { background:#294c6b; border:1px solid #5aa3dc; color:white; padding:9px 15px; font-weight:600; }
        QPushButton#danger { color:#ffb9b2; border-color:#a65b5b; }
        QPushButton#danger:hover { background:#59363d; border-color:#d47a7a; }
        QTabWidget::pane { border:1px solid #40546a; border-radius:9px; top:-1px; }
        QTabBar { qproperty-drawBase:0; }
        QTabBar::tab { background:#202b38; color:#aebfd2; border:1px solid #3b5065; border-radius:7px; padding:10px 17px; margin-right:6px; min-width:105px; }
        QTabBar::tab:hover { background:#2d4054; color:#ffffff; border-color:#6c91b5; }
        QTabBar::tab:selected { background:#294f72; color:white; border:1px solid #5da7df; }
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit {
            background:#202c39; color:#e4edf7; border:1px solid #4b6279;
            border-radius:6px; padding:6px 9px; selection-background-color:#286da6;
        }
        QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateEdit:hover, QTimeEdit:hover { border-color:#6c91b5; }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus { border:1px solid #69aee5; }
        QComboBox::drop-down, QDateEdit::drop-down { border:0; width:28px; background:transparent; }
        QComboBox QAbstractItemView {
            background:#202c39; color:#e7edf5; border:1px solid #526b84; outline:0;
            selection-background-color:#294f72; selection-color:#ffffff; padding:4px;
        }
        QComboBox QAbstractItemView::item { min-height:26px; padding:5px 9px; }
        QComboBox QAbstractItemView::item:hover { background:#30465c; color:#ffffff; }
        QCheckBox { spacing:8px; color:#dce6f2; background:transparent; }
        QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
        QCheckBox::indicator:checked { background:#2478bb; border:1px solid #75baf0; }
        QMenu { background:#243140; color:#e7edf5; border:1px solid #526b84; border-radius:7px; padding:4px; }
        QMenu::item { padding:8px 24px; border-radius:4px; }
        QMenu::item:selected { background:#315574; }
        QGroupBox { background:#253241; border:1px solid #4b6279; border-radius:8px; margin-top:12px; padding:12px; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; color:#dce8f5; }
        QScrollBar:vertical { background:#1d2733; width:11px; margin:3px; border-radius:5px; }
        QScrollBar::handle:vertical { background:#526b84; border-radius:5px; min-height:26px; }
        QScrollBar::handle:vertical:hover { background:#7298bc; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:#1d2733; height:11px; margin:3px; border-radius:5px; }
        QScrollBar::handle:horizontal { background:#526b84; border-radius:5px; min-width:26px; }
        QToolTip { background:#263748; color:#f0f6fc; border:1px solid #6b8ba9; padding:5px 8px; border-radius:5px; }
        QStatusBar { background:#18212b; color:#9fb1c6; border-top:1px solid #34485c; }
    )");

    QMainWindow window;
    window.setWindowTitle("RRS RM Launcher");
    window.resize(1920, 1080);
    window.setMinimumSize(800, 600);

    auto *root = new QWidget;
    auto *mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    auto *accountPanel = makePanel();
    auto *accountLayout = new QHBoxLayout(accountPanel);
    accountLayout->setContentsMargins(10, 6, 12, 6);
    accountLayout->setSpacing(6);

    auto *mainMenuGroup = new QButtonGroup(&window);
    mainMenuGroup->setExclusive(true);
    const QStringList mainMenuTitles = {"Главная", "Мультиплеер", "Журналы", "Конфигуратор поездов", "Дополнения", "Загрузки", "Скриншоты", "Настройки"};
    // Индексы иконок соответствуют прежней таблице рисунков makeMenuIcon().
    const QList<int> mainMenuIconIndices = {0, 1, 7, 5, 6, 2, 3, 4};
    QList<QPushButton*> mainMenuButtons;
    for (int i = 0; i < mainMenuTitles.size(); ++i) {
        auto *button = new QPushButton(mainMenuTitles[i]);
        button->setObjectName("navButton");
        button->setIcon(makeMenuIcon(mainMenuIconIndices[i]));
        button->setIconSize(QSize(21, 21));
        button->setMinimumHeight(42);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        mainMenuGroup->addButton(button);
        accountLayout->addWidget(button);
        mainMenuButtons.append(button);
    }
    mainMenuButtons.first()->setChecked(true);
    accountLayout->addStretch(1);

    auto *accountStack = new QStackedWidget;
    accountStack->setFixedHeight(40);
    accountStack->setFixedWidth(92);
    auto *loginButton = new QPushButton("Войти");
    loginButton->setObjectName("primary");
    loginButton->setFixedHeight(38);
    loginButton->setMinimumWidth(0);
    accountStack->addWidget(loginButton);

    auto *profile = new QWidget;
    auto *profileLayout = new QHBoxLayout(profile);
    profileLayout->setContentsMargins(8, 0, 8, 0);
    auto *avatar = new QLabel("●");
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(28, 28);
    avatar->setStyleSheet("background:#354b68; border-radius:14px;");
    auto *nickname = new QLabel("Никнейм");
    QFont nicknameFont = nickname->font();
    nicknameFont.setBold(true);
    nickname->setFont(nicknameFont);
    profileLayout->addWidget(avatar);
    profileLayout->addWidget(nickname);
    accountStack->addWidget(profile);
    accountLayout->addWidget(accountStack);
    mainLayout->addWidget(accountPanel);

    auto *pages = new QStackedWidget;
    mainLayout->addWidget(pages, 1);

    // Главная страница симулятора
    auto *buildsPage = new QWidget;
    auto *buildsLayout = new QVBoxLayout(buildsPage);
    buildsLayout->setContentsMargins(4, 4, 4, 4);
    buildsLayout->setSpacing(14);

    auto *workspace = makePanel();
    auto *workLayout = new QVBoxLayout(workspace);
    workLayout->setContentsMargins(14, 12, 14, 14);
    workLayout->setSpacing(12);

    // Блок запуска сервера размещается справа в строке выбора маршрута.
    auto *launchGroup = new QWidget;
    auto *launchGroupLayout = new QHBoxLayout(launchGroup);
    launchGroupLayout->setContentsMargins(0, 0, 0, 0);
    launchGroupLayout->setSpacing(0);
    auto *launch = new QPushButton("Запустить сервер");
    launch->setObjectName("launchMain");
    launch->setFixedSize(168, 52);
    auto *more = new QToolButton;
    more->setObjectName("launchArrow");
    more->setText(QString());
    more->setToolButtonStyle(Qt::ToolButtonTextOnly);
    more->setPopupMode(QToolButton::InstantPopup);
    more->setFixedSize(30, 52);
    more->setStyleSheet("QToolButton#launchArrow { background:#2878c7; color:white; border:1px solid #398bd9; border-left:0; border-top-right-radius:6px; border-bottom-right-radius:6px; border-top-left-radius:0; border-bottom-left-radius:0; padding:0; text-align:right; } QToolButton#launchArrow::menu-indicator { subcontrol-origin: padding; subcontrol-position: right bottom; right:8px; bottom:5px; } QToolButton#launchArrow:hover { background:#398bd9; border-color:#62a7e8; } QToolButton#launchArrow:pressed { background:#2166a8; border-color:#398bd9; }");
    launch->setStyleSheet("QPushButton#launchMain { background:#2878c7; color:white; border:1px solid #398bd9; border-right:0; border-top-left-radius:6px; border-bottom-left-radius:6px; border-top-right-radius:0; border-bottom-right-radius:0; font-weight:bold; padding:5px 9px; } QPushButton#launchMain:hover { background:#398bd9; border-color:#62a7e8; } QPushButton#launchMain:pressed { background:#2166a8; border-color:#398bd9; }");
    auto *launchMenu = new QMenu(more);
    QAction *launchViewerAction = launchMenu->addAction("Запустить Viewer");
    QAction *launchRouteMapAction = launchMenu->addAction("Запустить карту маршрута");
    more->setMenu(launchMenu);
    launchGroupLayout->addWidget(launch);
    launchGroupLayout->addWidget(more);
    // Выбор маршрута симулятора. Названия берём из description.xml
    // (узел Config/Route/Title), а при отсутствии файла используем имя папки.
    const QString routesDirectory = QDir(simulatorRoot).filePath("routes");
    const QFileInfoList routeDirectories = QDir(routesDirectory).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);

    auto *routeSelectionPanel = makePanel();
    auto *routeSelectionLayout = new QHBoxLayout(routeSelectionPanel);
    routeSelectionLayout->setContentsMargins(12, 10, 12, 10);
    routeSelectionLayout->setSpacing(10);
    auto *routeSelectionLabel = new QLabel("Маршрут:");
    routeSelectionLabel->setStyleSheet("background:transparent; font-weight:600;");
    auto *routeSelector = new MainSettingsComboBox;
    routeSelector->setObjectName("mainRouteSelector");
    routeSelector->setMinimumHeight(42);
    routeSelector->setFixedWidth(500);
    routeSelector->addItem("Выберите маршрут...", QString());
    for (const QFileInfo &routeInfo : routeDirectories) {
        QString routeTitle = routeInfo.fileName();
        QString descriptionPath = QDir(routeInfo.absoluteFilePath()).filePath("description.xml");
        if (!QFileInfo::exists(descriptionPath))
            descriptionPath = QDir(routeInfo.absoluteFilePath()).filePath("description");
        QFile descriptionFile(descriptionPath);
        if (descriptionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QXmlStreamReader routeXml(&descriptionFile);
            bool inRoute = false;
            while (!routeXml.atEnd()) {
                routeXml.readNext();
                if (routeXml.isStartElement()) {
                    if (routeXml.name() == QStringLiteral("Route")) inRoute = true;
                    else if (inRoute && routeXml.name() == QStringLiteral("Title")) {
                        const QString title = routeXml.readElementText().trimmed();
                        if (!title.isEmpty()) routeTitle = title;
                    }
                } else if (routeXml.isEndElement() && routeXml.name() == QStringLiteral("Route")) {
                    inRoute = false;
                }
            }
        }
        routeSelector->addItem(routeTitle, routeInfo.absoluteFilePath());
    }
    if (routeDirectories.isEmpty()) {
        routeSelector->setItemText(0, "В каталоге routes нет маршрутов");
        routeSelector->setEnabled(false);
    }

    auto *scenarioDate = new GraphicsDateEdit(QDate::currentDate());
    scenarioDate->setCalendarPopup(true);
    scenarioDate->setDisplayFormat("dd.MM.yyyy");
    scenarioDate->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    scenarioDate->setMinimumHeight(32);
    scenarioDate->setFixedWidth(140);
    auto *scenarioTime = new GraphicsTimeEdit(QTime::currentTime());
    scenarioTime->setObjectName("scenarioTime");
    scenarioTime->setDisplayFormat("HH:mm:ss");
    scenarioTime->setTime(QTime::currentTime());
    scenarioTime->setMinimumHeight(32);
    scenarioTime->setMinimumWidth(105);
    auto *scenarioSystemTime = new GraphicsCheckBox("Системное время");
    scenarioSystemTime->setStyleSheet(R"(
        QCheckBox { background:transparent; color:#e7edf5; spacing:8px; }
        QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
        QCheckBox::indicator:checked { background:#202c39; border:1px solid #75baf0; }
        QCheckBox::indicator:checked:hover { background:#30465c; border:1px solid #b5dcff; }
        QCheckBox::indicator:unchecked:hover { border:1px solid #9bcdf4; background:#30465c; }
    )");
    scenarioSystemTime->setTristate(false);
    scenarioSystemTime->setCheckable(true);
    scenarioSystemTime->setChecked(true);
    scenarioSystemTime->setCursor(Qt::PointingHandCursor);
    auto updateScenarioDateTimeEnabled = [scenarioDate, scenarioTime](bool useSystemTime) {
        scenarioDate->setEnabled(!useSystemTime);
        scenarioTime->setEnabled(!useSystemTime);
    };
    QObject::connect(scenarioSystemTime, &QCheckBox::toggled, &window,
        updateScenarioDateTimeEnabled);
    // Применяем состояние и при инициализации, и при каждом нажатии галочки.
    updateScenarioDateTimeEnabled(scenarioSystemTime->isChecked());

    auto *startDateLabel = new QLabel("Дата старта:");
    startDateLabel->setStyleSheet("background:transparent; font-weight:600;");
    auto *startTimeLabel = new QLabel("Время:");
    startTimeLabel->setStyleSheet("background:transparent; font-weight:600;");
    routeSelectionLayout->addWidget(routeSelectionLabel);
    routeSelectionLayout->addWidget(routeSelector);
    routeSelectionLayout->addStretch(1);
    routeSelectionLayout->addWidget(launchGroup, 0, Qt::AlignRight | Qt::AlignVCenter);
    workLayout->addWidget(routeSelectionPanel);

    auto *sectionPages = new QStackedWidget;
    sectionPages->setStyleSheet("QStackedWidget { background:transparent; border:0; }");
    auto *scenarioPage = new QWidget;
    scenarioPage->setStyleSheet("QWidget { background:transparent; }");
    auto *scenarioLayout = new QVBoxLayout(scenarioPage);
    scenarioLayout->setContentsMargins(0, 4, 0, 4);
    scenarioLayout->setSpacing(12);

    // Горизонтальная компоновка: выбор сценария слева, генератор справа.
    auto *scenarioEditorColumn = new QHBoxLayout;
    scenarioEditorColumn->setContentsMargins(0, 6, 0, 6);
    scenarioEditorColumn->setSpacing(12);

    auto *scenarioFormPanel = makePanel();
    scenarioFormPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *scenarioFormLayout = new QVBoxLayout(scenarioFormPanel);
    scenarioFormLayout->setContentsMargins(14, 9, 14, 9);
    scenarioFormLayout->setSpacing(6);
    auto *scenarioFormTitle = heading("Генерация свободной поездки");
    scenarioFormTitle->setStyleSheet("background:transparent;");
    scenarioFormLayout->addWidget(scenarioFormTitle);

    // Все параметры свободной поездки располагаются вертикально, друг под другом.
    auto *scenarioForm = new QFormLayout;
    scenarioForm->setHorizontalSpacing(12);
    scenarioForm->setVerticalSpacing(6);
    scenarioForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    scenarioForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto makeScenarioCombo = [](const QStringList &items) {
        auto *combo = new MainSettingsComboBox;
        combo->addItems(items);
        combo->setMinimumHeight(29);
        combo->setFixedWidth(650);
        combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        return combo;
    };
    auto *scenarioTrain = makeScenarioCombo({"Выберите поезд..."});
    // Для генерации свободной поездки используем только cfg/trains.
    const QString generatorTrainsDirectory = QDir(simulatorRoot).filePath("cfg/trains");
    const QFileInfoList generatorTrainFiles = QDir(generatorTrainsDirectory).entryInfoList(
        {"*.xml"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &trainInfo : generatorTrainFiles) {
        QFile trainFile(trainInfo.absoluteFilePath());
        if (!trainFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QXmlStreamReader trainXml(&trainFile);
        QString trainTitle;
        bool inCommon = false;
        while (!trainXml.atEnd()) {
            trainXml.readNext();
            if (trainXml.isStartElement() && trainXml.name() == QStringLiteral("Common"))
                inCommon = true;
            else if (trainXml.isStartElement() && inCommon && trainXml.name() == QStringLiteral("Title"))
                trainTitle = trainXml.readElementText().trimmed();
            else if (trainXml.isEndElement() && trainXml.name() == QStringLiteral("Common"))
                inCommon = false;
        }
        if (trainXml.hasError()) continue;
        if (trainTitle.isEmpty()) trainTitle = trainInfo.completeBaseName();
        scenarioTrain->addItem(trainTitle, trainInfo.absoluteFilePath());
    }
    auto *scenarioStartPoint = makeScenarioCombo({QString()});
    auto *scenarioStartDirection = makeScenarioCombo({"Прямое", "Обратное"});
    // По умолчанию движение задано в прямом направлении.
    scenarioStartDirection->setCurrentIndex(0);
    auto *scenarioTrajectory = makeScenarioCombo({QString()});

    // Для длинных списков точек и траекторий разрешаем ввод текста.
    // Completer ищет совпадения по любой части названия, без учёта регистра;
    // новые значения в список не добавляются.
    auto enableScenarioComboSearch = [](QComboBox *combo) {
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setDuplicatesEnabled(false);
        if (QLineEdit *edit = combo->lineEdit()) {
            edit->setClearButtonEnabled(false);
            edit->setPlaceholderText(QString());
            edit->clear();
        }
        if (QCompleter *completer = combo->completer()) {
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            completer->setFilterMode(Qt::MatchContains);
            completer->setCompletionMode(QCompleter::PopupCompletion);
        }
    };
    enableScenarioComboSearch(scenarioStartPoint);
    enableScenarioComboSearch(scenarioTrajectory);
    auto *scenarioTrajectoryDirection = makeScenarioCombo({"Выберите направление...", "Прямое", "Обратное"});
    auto *scenarioCoordinate = new GraphicsDoubleSpinBox;
    scenarioCoordinate->setObjectName("scenarioCoordinate");
    scenarioCoordinate->setRange(-1000000000.0, 1000000000.0);
    scenarioCoordinate->setDecimals(3);
    scenarioCoordinate->setValue(0.0);
    scenarioCoordinate->setSingleStep(1.0);
    scenarioCoordinate->setMinimumHeight(29);
    scenarioCoordinate->setProperty("coordinateConfirmed", false);
    QObject::connect(scenarioCoordinate, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        scenarioCoordinate, [scenarioCoordinate](double) { scenarioCoordinate->setProperty("coordinateConfirmed", true); });
    auto addScenarioFormRow = [scenarioForm](const QString &caption, QWidget *field) {
        auto *label = new QLabel(caption);
        label->setMinimumWidth(300); // увеличиваем только левую колонку: поля смещаются вправо
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setStyleSheet("background:transparent;");
        scenarioForm->addRow(label, field);
        // Выравниваем только сами поля по правому краю их колонки.
        // Названия параметров остаются на прежнем месте слева.
        scenarioForm->setAlignment(field, Qt::AlignRight | Qt::AlignVCenter);
    };
    addScenarioFormRow("Поезд:", scenarioTrain);
    addScenarioFormRow("Направление движения:", scenarioStartDirection);
    addScenarioFormRow("Точка старта:", scenarioStartPoint);
    addScenarioFormRow("Стартовая траектория:", scenarioTrajectory);
    addScenarioFormRow("Направление на стартовой траектории:", scenarioTrajectoryDirection);
    addScenarioFormRow("Координата:", scenarioCoordinate);
    auto *generatorBodyLayout = new QVBoxLayout;
    generatorBodyLayout->setSpacing(10);
    auto *configurationColumn = new QVBoxLayout;
    configurationColumn->setSpacing(8);
    configurationColumn->addLayout(scenarioForm);
    generatorBodyLayout->addLayout(configurationColumn, 0);

    auto *addedTrainsPanel = makePanel();
    addedTrainsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    addedTrainsPanel->setMinimumWidth(250);
    auto *addedTrainsLayout = new QVBoxLayout(addedTrainsPanel);
    addedTrainsLayout->setContentsMargins(10, 8, 10, 8);
    addedTrainsLayout->setSpacing(7);
    auto *addedTrainsTitle = heading("Добавленные поезда");
    addedTrainsTitle->setStyleSheet("background:transparent;");
    addedTrainsLayout->addWidget(addedTrainsTitle);

    auto *addedTrainsList = new QListWidget;
    addedTrainsList->setMinimumHeight(90);
    addedTrainsList->setStyleSheet(R"(
        QListWidget { background:#303640; border:1px solid #465363; border-radius:6px; }
        QListWidget::item { background:transparent; color:#b8c9df; padding:2px 4px; margin:2px; border:0; border-radius:5px; }
        QListWidget::item:hover { background:rgba(155,188,229,24); }
        QListWidget::item:selected { background:#285b8e; color:white; }
    )");
    addedTrainsLayout->addWidget(addedTrainsList, 1);

    auto *startDateTimeRow = new QHBoxLayout;
    startDateTimeRow->setSpacing(8);
    startDateLabel->setStyleSheet("background:transparent; font-weight:600;");
    startTimeLabel->setStyleSheet("background:transparent; font-weight:600;");
    startDateTimeRow->addWidget(startDateLabel);
    startDateTimeRow->addWidget(scenarioDate);
    startDateTimeRow->addWidget(startTimeLabel);
    startDateTimeRow->addWidget(scenarioTime);
    startDateTimeRow->addWidget(scenarioSystemTime);
    startDateTimeRow->addStretch(1);
    addedTrainsLayout->addLayout(startDateTimeRow);

    // До выбора маршрута блокируем все параметры генератора, включая дату,
    // время и системное время. Уведомление появляется при попытке взаимодействия.
    auto *routeSelectionGuard = new RouteSelectionGuard(routeSelector, &window);
    const QList<QWidget *> routeDependentControls = {
        scenarioTrain, scenarioStartDirection, scenarioStartPoint, scenarioTrajectory,
        scenarioTrajectoryDirection, scenarioCoordinate, scenarioDate, scenarioTime,
        scenarioSystemTime
    };
    for (QWidget *control : routeDependentControls) {
        control->installEventFilter(routeSelectionGuard);
        // Элементы остаются визуально в обычном активном стиле; блокируется
        // только взаимодействие через RouteSelectionGuard до выбора маршрута.
        control->setProperty("routeDependentControl", true);
    }
    // Редакторы внутри полей даты, времени и координаты тоже должны быть защищены.
    for (QWidget *control : routeDependentControls) {
        const auto children = control->findChildren<QWidget *>();
        for (QWidget *child : children)
            child->installEventFilter(routeSelectionGuard);
    }

    auto *addedTrainButtons = new QHBoxLayout;
    addedTrainButtons->setSpacing(8);
    auto *addConfiguredTrainButton = new QPushButton("＋ Добавить");
    auto *removeConfiguredTrainButton = new QPushButton("− Удалить");
    for (auto *button : {addConfiguredTrainButton, removeConfiguredTrainButton}) {
        button->setObjectName("scenarioActionButton");
        button->setStyleSheet(scenarioActionButtonStyle);
        button->setMinimumHeight(32);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        addedTrainButtons->addWidget(button);
    }
    removeConfiguredTrainButton->setEnabled(false);
    // Порядок сверху вниз: поля, кнопки, список добавленных поездов.
    addedTrainButtons->setAlignment(Qt::AlignLeft);
    configurationColumn->addLayout(addedTrainButtons);
    configurationColumn->addWidget(addedTrainsPanel, 1);

    scenarioFormLayout->addLayout(generatorBodyLayout);
    scenarioEditorColumn->addWidget(scenarioFormPanel, 0);

    QObject::connect(addConfiguredTrainButton, &QPushButton::clicked, &window,
        [scenarioTrain, scenarioStartPoint, scenarioTrajectory, scenarioTrajectoryDirection, scenarioCoordinate,
         addedTrainsList, removeConfiguredTrainButton]() {
            if (scenarioTrain->currentIndex() <= 0) {
                QMessageBox::information(nullptr, "Добавление поезда", "Сначала выберите поезд в поле «Поезд».");
                return;
            }

            if (scenarioTrajectory->currentIndex() <= 0) {
                QMessageBox::information(nullptr, "Добавление поезда", "Выберите стартовую траекторию или точку старта.");
                return;
            }
            const QString trajectory = scenarioTrajectory->currentText();
            // Сначала проверяем занятость траектории: повторное нажатие «Добавить»
            // должно сразу сообщать о конфликте, не требуя повторного подтверждения координаты.
            for (int i = 0; i < addedTrainsList->count(); ++i) {
                const QListWidgetItem *existingItem = addedTrainsList->item(i);
                if (existingItem && existingItem->data(Qt::UserRole + 1).toString() == trajectory) {
                    QMessageBox::information(nullptr, "Добавление поезда",
                        QString("Стартовая траектория «%1» уже занята другим поездом.").arg(trajectory));
                    return;
                }
            }

            if (scenarioTrajectoryDirection->currentIndex() <= 0) {
                QMessageBox::information(nullptr, "Добавление поезда", "Выберите направление на стартовой траектории.");
                return;
            }
            if (!scenarioCoordinate->property("coordinateConfirmed").toBool()) {
                QMessageBox::information(nullptr, "Добавление поезда", "Укажите координату старта.");
                return;
            }

            const bool usesStartPoint = scenarioStartPoint->currentIndex() > 0;
            const QString startPointName = usesStartPoint ? scenarioStartPoint->currentText() : QString();
            const QString trajectoryDirection = scenarioTrajectoryDirection->currentText();
            const QString coordinate = QLocale().toString(scenarioCoordinate->value(), 'f', 3);

            auto *configuredTrainItem = new QListWidgetItem;
            configuredTrainItem->setData(Qt::UserRole, scenarioTrain->currentData());
            configuredTrainItem->setData(Qt::UserRole + 1, trajectory);
            configuredTrainItem->setData(Qt::UserRole + 2, trajectoryDirection);
            configuredTrainItem->setData(Qt::UserRole + 3, scenarioCoordinate->value());
            configuredTrainItem->setData(Qt::UserRole + 4, usesStartPoint);
            configuredTrainItem->setData(Qt::UserRole + 5, startPointName);
            addedTrainsList->addItem(configuredTrainItem);

            // Имя поезда слева, сохранённые параметры движения справа.
            auto *rowWidget = new QWidget(addedTrainsList);
            rowWidget->setMinimumHeight(40);
            rowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            rowWidget->setStyleSheet("background:transparent;");
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(8, 0, 8, 0);
            rowLayout->setSpacing(12);
            rowLayout->setAlignment(Qt::AlignVCenter);

            auto *trainNameLabel = new QLabel(scenarioTrain->currentText(), rowWidget);
            trainNameLabel->setFont(addedTrainsList->font());
            trainNameLabel->setStyleSheet("background:transparent; color:#e7edf5;");
            trainNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            trainNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

            const QString leadingLocation = usesStartPoint ? startPointName : trajectory;
            auto *trainDetailsLabel = new QLabel(
                QString("%1  ·  %2  ·  %3").arg(leadingLocation, trajectoryDirection, coordinate), rowWidget);
            trainDetailsLabel->setFont(addedTrainsList->font());
            trainDetailsLabel->setStyleSheet("background:transparent; color:#c0ccda;");
            trainDetailsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            trainDetailsLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

            rowLayout->addWidget(trainNameLabel, 1);
            rowLayout->addWidget(trainDetailsLabel, 0);
            configuredTrainItem->setSizeHint(QSize(0, 42));
            addedTrainsList->setItemWidget(configuredTrainItem, rowWidget);

            addedTrainsList->setCurrentItem(configuredTrainItem);
            removeConfiguredTrainButton->setEnabled(true);
            // Сохранённая координата остаётся подтверждённой для следующего поезда.
            // Повторный ввод не требуется, если пользователь не менял её значение.
        });
    QObject::connect(removeConfiguredTrainButton, &QPushButton::clicked, &window,
        [addedTrainsList, removeConfiguredTrainButton]() {
            const int row = addedTrainsList->currentRow();
            if (row < 0) return;
            delete addedTrainsList->takeItem(row);
            removeConfiguredTrainButton->setEnabled(addedTrainsList->count() > 0);
        });
    QObject::connect(addedTrainsList, &QListWidget::currentRowChanged, &window,
        [removeConfiguredTrainButton](int row) {
            removeConfiguredTrainButton->setEnabled(row >= 0);
        });

    // Нижняя плашка под генератором: выбор сценария и описание.
    // Она намеренно компактнее основной зоны генерации свободной поездки.
    auto *scenarioInfoPanel = makePanel();
    scenarioInfoPanel->setMinimumHeight(190);
    scenarioInfoPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *scenarioInfoLayout = new QVBoxLayout(scenarioInfoPanel);
    scenarioInfoLayout->setContentsMargins(16, 12, 16, 12);
    scenarioInfoLayout->setSpacing(22);

    // Сначала список сценариев, ниже — описание выбранного сценария.
    auto *scenarioSelectionColumn = new QVBoxLayout;
    scenarioSelectionColumn->setSpacing(6);
    auto *scenarioListTitle = heading("Выбор сценария");
    scenarioListTitle->setStyleSheet("background:transparent;");
    scenarioSelectionColumn->addWidget(scenarioListTitle);

    auto *scenarios = new QListWidget;
    scenarios->setMinimumHeight(100);
    scenarios->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scenarios->setStyleSheet(R"(
        QListWidget { background:#303640; border:1px solid #465363; border-radius:6px; }
        QListWidget::item { background:transparent; color:#b8c9df; padding:10px 10px; margin:3px; border:0; border-radius:5px; }
        QListWidget::item:hover { background:rgba(155,188,229,24); }
        QListWidget::item:selected { background:#354b68; color:white; }
    )");
    // Сценарии загружаются после выбора маршрута. По умолчанию ничего не выбрано.
    scenarios->setSelectionMode(QAbstractItemView::SingleSelection);
    scenarios->setCurrentRow(-1);
    scenarioSelectionColumn->addWidget(scenarios, 1);
    // Верхняя область со списком занимает примерно половину высоты плашки.
    scenarioInfoLayout->addLayout(scenarioSelectionColumn, 1);

    auto *scenarioDescriptionWidget = new QWidget;
    auto *scenarioDescriptionColumn = new QVBoxLayout(scenarioDescriptionWidget);
    scenarioDescriptionColumn->setContentsMargins(0, 0, 0, 0);
    scenarioDescriptionColumn->setSpacing(6);
    auto *detailsTitle = heading("Описание");
    detailsTitle->setStyleSheet("background:transparent;");
    scenarioDescriptionColumn->addWidget(detailsTitle);

    auto *detailsText = new QLabel;
    detailsText->setWordWrap(true);
    detailsText->setObjectName("muted");
    detailsText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailsText->setStyleSheet("background:transparent;");
    scenarioDescriptionColumn->addWidget(detailsText, 1);
    // Блок описания всегда занимает своё место, даже если сценарий не выбран.
    scenarioInfoLayout->addWidget(scenarioDescriptionWidget, 1);

    // Выбор сценария располагается первым, слева; генератор — справа.
    scenarioEditorColumn->insertWidget(0, scenarioInfoPanel, 1);

    // Обе колонки делят доступную ширину поровну.
    scenarioEditorColumn->setStretch(0, 1);
    scenarioEditorColumn->setStretch(1, 1);
    scenarioLayout->addLayout(scenarioEditorColumn, 1);
    sectionPages->addWidget(scenarioPage);

    auto makeTabPage = [](const QString &title, const QString &message) {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(8, 8, 8, 8);
        auto *label = new QLabel(message);
        label->setAlignment(Qt::AlignCenter);
        label->setObjectName("muted");
        label->setWordWrap(true);
        layout->addWidget(label, 1);
        return page;
    };
    auto *trainsPage = new QWidget;
    trainsPage->setObjectName("trainsPage");
    trainsPage->setStyleSheet("QWidget#trainsPage { background:transparent; }");
    auto *trainsRoot = new QVBoxLayout(trainsPage);
    trainsRoot->setContentsMargins(4, 4, 4, 4);
    trainsRoot->setSpacing(10);

    auto makeTrainPanel = [](const QString &title, QLabel **headerOut = nullptr) {
        auto *panel = makePanel();
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(6);
        auto *header = heading(title);
        header->setStyleSheet("background:transparent;");
        layout->addWidget(header);
        if (headerOut) *headerOut = header;
        return qMakePair(panel, layout);
    };
    auto makeTrainSearch = [](const QString &placeholder) {
        auto *edit = new QLineEdit;
        edit->setPlaceholderText(placeholder);
        edit->setMinimumHeight(32);
        edit->setStyleSheet("QLineEdit { background:#292e36; color:#e7eaf0; border:1px solid #4b5563; border-radius:5px; padding:5px 8px; } QLineEdit:focus { border-color:#6b9acb; }");
        return edit;
    };
    auto makeTrainList = [] {
        auto *list = new QListWidget;
        list->setStyleSheet(R"(
            QListWidget { background:#303640; border:1px solid #465363; border-radius:6px; }
            QListWidget::item { background:transparent; color:#b8c9df; padding:3px 8px; margin:1px; border:0; border-radius:5px; }
            QListWidget::item:hover { background:rgba(155,188,229,24); }
            QListWidget::item:selected { background:#285b8e; color:white; }
        )");
        return list;
    };

    auto *upperTrains = new QHBoxLayout;
    upperTrains->setSpacing(10);

    auto [createdTrainsPanel, createdTrainsLayout] = makeTrainPanel("Созданные поезда");
    auto *createdTrainSearch = makeTrainSearch("Поиск по поездам...");
    auto *createdTrains = makeTrainList();
    createdTrainsLayout->addWidget(createdTrainSearch);
    createdTrainsLayout->addWidget(createdTrains, 1);
    createdTrainsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    upperTrains->addWidget(createdTrainsPanel, 1);

    auto [unitsPanel, unitsLayout] = makeTrainPanel("Подвижные единицы");
    auto *unitSearch = makeTrainSearch("Поиск по единицам...");
    auto *unitType = new MainSettingsComboBox;
    unitType->addItems({"Все типы", "Локомотивы", "Пассажирские вагоны", "Грузовые вагоны", "Секции"});
    unitType->setMinimumHeight(32);
    auto *units = makeTrainList();
    unitsLayout->addWidget(unitSearch);
    unitsLayout->addWidget(unitType);
    unitsLayout->addWidget(units, 1);
    unitsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    upperTrains->addWidget(unitsPanel, 1);

    QLabel *unitDetailsTitle = nullptr;
    auto [unitDetailsPanel, unitDetailsLayout] = makeTrainPanel("ВЛ80с-1274", &unitDetailsTitle);
    auto *unitPreview = new GltfPreviewWidget;
    unitDetailsLayout->addWidget(unitPreview, 1);
    auto *unitSpecs = new QLabel;
    unitSpecs->setTextFormat(Qt::RichText);
    unitSpecs->setWordWrap(true);
    unitSpecs->setStyleSheet("QLabel { background:#252c35; color:#cbd5e1; border:1px solid #465363; border-radius:6px; padding:8px; }");
    auto setUnitSpecs = [unitSpecs](const QString &type, const QString &mass,
                                   const QString &payloadMass, const QString &length,
                                   const QString &axes, const QString &couplings,
                                   const QString &wheelDiameter) {
        const QString cellStyle = "padding:3px 10px 3px 0; vertical-align:top;";
        auto cell = [&cellStyle](const QString &label, const QString &value) {
            return QString("<td style=\"%1\"><span style=\"color:#8fa4bc;\">%2:</span> %3</td>")
                .arg(cellStyle, label.toHtmlEscaped(), value.toHtmlEscaped());
        };
        unitSpecs->setText(QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">")
            + "<tr>" + cell("Тип", type) + cell("Масса", mass) + "</tr>"
            + "<tr>" + cell("Масса груза", payloadMass) + cell("Длина", length) + "</tr>"
            + "<tr>" + cell("Количество осей", axes) + cell("Тип автосцепок", couplings) + "</tr>"
            + "<tr>" + cell("Диаметр колёс", wheelDiameter) + "<td></td></tr></table>");
    };
    // Характеристики слева; справа по центру — галочки и вертикальная колонка кнопок.
    auto *unitSpecsOptionsRow = new QHBoxLayout;
    unitSpecsOptionsRow->setSpacing(18);
    unitSpecs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    unitSpecsOptionsRow->addWidget(unitSpecs, 1);

    auto *unitControlsColumn = new QVBoxLayout;
    unitControlsColumn->setSpacing(0);
    unitControlsColumn->addStretch(1);

    auto *unitControlsRow = new QHBoxLayout;
    unitControlsRow->setSpacing(16);

    auto *unitOptionsColumn = new QVBoxLayout;
    unitOptionsColumn->setSpacing(12);
    auto *emptyLoadCheck = new GraphicsCheckBox("Порожний");
    auto *reverseOrientationCheck = new GraphicsCheckBox("Обратная ориентация");
    auto *brakeShoesCheck = new GraphicsCheckBox("На башмаках");
    for (auto *check : {emptyLoadCheck, reverseOrientationCheck, brakeShoesCheck}) {
        // Используем тот же стиль индикатора, что и на вкладке «Настройки»:
        // стандартная галочка Qt без собственной заливки/иконки.
        check->setStyleSheet(R"(
            QCheckBox { background:transparent; color:#e7edf5; spacing:8px; }
            QCheckBox::indicator {
                width:16px; height:16px;
                border:1px solid #718ba5;
                border-radius:4px;
                background:#202c39;
            }
            QCheckBox::indicator:checked {
                background:#202c39;
                border:1px solid #75baf0;
            }
            QCheckBox::indicator:unchecked:hover {
                border:1px solid #9bcdf4;
                background:#30465c;
            }
        )");
        check->setCursor(Qt::PointingHandCursor);
        unitOptionsColumn->addWidget(check);
    }
    unitControlsRow->addLayout(unitOptionsColumn);

    auto *unitActionsColumn = new QVBoxLayout;
    unitActionsColumn->setSpacing(8);
    auto *addUnitButton = new QPushButton("＋  Добавить");
    auto *removeUnitButton = new QPushButton("▤  Удалить");
    for (auto *button : {addUnitButton, removeUnitButton}) {
        button->setObjectName("scenarioActionButton");
        button->setStyleSheet(scenarioActionButtonStyle);
        button->setMinimumWidth(132);
        button->setMinimumHeight(36);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        button->setCursor(Qt::PointingHandCursor);
        unitActionsColumn->addWidget(button);
    }
    unitControlsRow->addLayout(unitActionsColumn);
    unitControlsColumn->addLayout(unitControlsRow);
    unitControlsColumn->addStretch(1);

    unitSpecsOptionsRow->addLayout(unitControlsColumn);
    unitDetailsLayout->addLayout(unitSpecsOptionsRow);

    unitDetailsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    upperTrains->addWidget(unitDetailsPanel, 2);
    trainsRoot->addLayout(upperTrains, 6);

    auto *lowerTrains = new QHBoxLayout;
    lowerTrains->setSpacing(10);
    auto [consistPanel, consistLayout] = makeTrainPanel("Состав поезда");
    auto *consistStrip = new ResponsiveConsistListWidget;
    consistStrip->setViewMode(QListView::IconMode);
    consistStrip->setFlow(QListView::LeftToRight);
    consistStrip->setWrapping(false);
    consistStrip->setMovement(QListView::Static);
    consistStrip->setResizeMode(QListView::Adjust);
    consistStrip->setIconSize(QSize(132, 88));
    consistStrip->setGridSize(QSize(154, 148));
    consistStrip->setSpacing(6);
    consistStrip->setWordWrap(true);
    consistStrip->setTextElideMode(Qt::ElideRight);
    consistStrip->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    consistStrip->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    consistStrip->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    consistStrip->horizontalScrollBar()->setSingleStep(40);
    consistStrip->horizontalScrollBar()->setPageStep(240);
    consistStrip->setStyleSheet(R"(
        QListWidget {
            background:#20262e;
            border:1px solid #465363;
            border-radius:6px;
        }
        QListWidget::item {
            background:#303d4e;
            color:#e7eaf0;
            border:1px solid #52657d;
            border-radius:6px;
            padding:5px;
            margin:3px;
        }
        QListWidget::item:selected { background:#354b68; }
        QScrollBar:horizontal {
            background:#1b2530;
            height:12px;
            margin:3px 8px 3px 8px;
            border-radius:6px;
        }
        QScrollBar::handle:horizontal {
            background:#526b84;
            min-width:36px;
            border-radius:5px;
        }
        QScrollBar::handle:horizontal:hover { background:#7298bc; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width:0px;
            background:transparent;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background:transparent;
        }
    )");
    auto makeConsistPhotoPlaceholder = [] {
        QPixmap photo(264, 176);
        photo.fill(QColor("#202a35"));
        QPainter painter(&photo);
        painter.setRenderHint(QPainter::Antialiasing);
        QLinearGradient gradient(0, 0, 0, photo.height());
        gradient.setColorAt(0.0, QColor("#394b60"));
        gradient.setColorAt(1.0, QColor("#202a35"));
        painter.fillRect(photo.rect(), gradient);
        painter.setPen(QPen(QColor("#7892ad"), 3));
        painter.drawRoundedRect(QRect(18, 18, 228, 140), 8, 8);
        painter.setPen(QColor("#b8c9df"));
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(15);
        painter.setFont(font);
        painter.drawText(photo.rect(), Qt::AlignCenter, "ФОТО\nПОДВИЖНОЙ ЕДИНИЦЫ");
        return QIcon(photo);
    };
    const QIcon consistPhotoPlaceholder = makeConsistPhotoPlaceholder();
    auto addConsistUnit = [consistStrip, consistPhotoPlaceholder, simulatorRoot](QVariantMap vehicle) {
        const QString unitName = vehicle.value("ModuleConfig").toString();
        const QString configFolder = vehicle.value("ModuleConfigDir").toString();
        const int count = qMax(1, vehicle.value("Count", 1).toInt());
        vehicle["Count"] = 1;

        // Находим XML конфигурацию и путь основной GLTF-модели так же,
        // как это делается для большого 3D-просмотра выбранной единицы.
        const QString vehiclesDirectory = QDir(simulatorRoot).filePath("cfg/vehicles");
        QString configPath = QDir(vehiclesDirectory).filePath(
            QDir(configFolder).filePath(unitName + ".xml"));
        if (!QFileInfo::exists(configPath)) {
            QDirIterator it(vehiclesDirectory, {unitName + ".xml"}, QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (configFolder.isEmpty() || QFileInfo(candidate).dir().dirName() == configFolder) {
                    configPath = candidate;
                    break;
                }
            }
        }

        QString modelPath;
        const QString modelName = readPrimaryModelName(configPath);
        if (!modelName.isEmpty()) {
            QString relativeModelName = QDir::fromNativeSeparators(modelName.trimmed());
            while (relativeModelName.startsWith("./"))
                relativeModelName.remove(0, 2);
            if (!QDir::isAbsolutePath(relativeModelName) &&
                !QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(relativeModelName).hasMatch()) {
                modelPath = QDir::cleanPath(QDir(simulatorRoot).filePath(
                    "data/models/" + relativeModelName));
                if (!QFileInfo::exists(modelPath))
                    modelPath.clear();
            }
        }

        for (int i = 0; i < count; ++i) {
            auto *item = new QListWidgetItem;
            item->setSizeHint(QSize(154, 148));
            item->setToolTip(QString("%1\\nПорожний: %2\\nОбратная ориентация: %3\\nНа башмаках: %4")
                .arg(unitName)
                .arg(vehicle.value("PayloadCoeff", "1").toString() == "0" ? "да" : "нет")
                .arg(vehicle.value("IsOrientationForward", "1").toString() == "0" ? "да" : "нет")
                .arg(vehicle.value("IsBrakeShoes", "0").toString() == "1" ? "да" : "нет"));
            item->setData(Qt::UserRole, vehicle);
            consistStrip->addItem(item);

            auto *card = new QWidget;
            card->setAttribute(Qt::WA_TranslucentBackground, true);
            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(5, 5, 5, 4);
            cardLayout->setSpacing(3);

            if (!modelPath.isEmpty()) {
                const QString ktxPath = QDir(simulatorRoot).filePath("bin/ktx.exe");
                const QFileInfo modelInfo(modelPath);
                const QString thumbnailKey = modelInfo.canonicalFilePath() + "|" +
                    QString::number(modelInfo.lastModified().toMSecsSinceEpoch()) + "|" + ktxPath;
                // Фиксированный контейнер не позволяет карточке менять высоту,
                // когда GLTF-превью заменяется готовой миниатюрой.
                auto *thumbnailContainer = new QWidget(card);
                thumbnailContainer->setFixedSize(132, 88);
                auto *thumbnailStack = new QStackedWidget(thumbnailContainer);
                thumbnailStack->setGeometry(thumbnailContainer->rect());
                thumbnailStack->setFixedSize(132, 88);
                auto *thumbnailImage = new QLabel(thumbnailStack);
                thumbnailImage->setFixedSize(132, 88);
                thumbnailImage->setAlignment(Qt::AlignCenter);
                thumbnailImage->setStyleSheet("background:#202a35; border:1px solid #52657d; border-radius:5px;");
                thumbnailStack->addWidget(thumbnailImage);
                cardLayout->addWidget(thumbnailContainer, 0, Qt::AlignHCenter);

                auto &imageCache = GltfPreviewWidget::thumbnailCache();
                auto &pending = GltfPreviewWidget::pendingThumbnails();
                const auto cachedImage = imageCache.constFind(thumbnailKey);
                if (cachedImage != imageCache.constEnd()) {
                    thumbnailImage->setPixmap(*cachedImage);
                    thumbnailStack->setCurrentWidget(thumbnailImage);
                } else if (pending.contains(thumbnailKey)) {
                    pending[thumbnailKey].append(QPointer<QLabel>(thumbnailImage));
                    thumbnailStack->setCurrentWidget(thumbnailImage);
                } else {
                    pending.insert(thumbnailKey, {QPointer<QLabel>(thumbnailImage)});
                    auto *renderer = new GltfPreviewWidget(thumbnailStack);
                    renderer->setFixedSize(132, 88);
                    renderer->setThumbnailMode();
                    thumbnailStack->addWidget(renderer);
                    thumbnailStack->setCurrentWidget(renderer);
                    // Для каждой уникальной модели создаётся только один OpenGL-рендерер.
                    // После первого кадра его изображение становится общим для всех карточек.
                    QObject::connect(renderer, &QOpenGLWidget::frameSwapped, renderer,
                        [renderer, thumbnailKey, thumbnailImage, thumbnailStack]() {
                            QPointer<GltfPreviewWidget> safeRenderer(renderer);
                            QPointer<QLabel> safeImage(thumbnailImage);
                            // Захватываем кадр с небольшой задержкой: сразу после frameSwapped
                            // некоторые драйверы ещё возвращают пустой framebuffer.
                            auto capture = std::make_shared<std::function<void(int)>>();
                            QPointer<QStackedWidget> safeStack(thumbnailStack);
                            *capture = [safeRenderer, safeImage, safeStack, thumbnailKey, capture](int attempt) {
                                if (!safeRenderer) return;
                                const QPixmap pixmap = QPixmap::fromImage(safeRenderer->grabFramebuffer())
                                    .scaled(132, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                                if (pixmap.isNull()) {
                                    if (attempt < 20) {
                                        QTimer::singleShot(50, safeRenderer, [capture, attempt]() {
                                            (*capture)(attempt + 1);
                                        });
                                    }
                                    return;
                                }
                                auto &cache = GltfPreviewWidget::thumbnailCache();
                                cache.insert(thumbnailKey, pixmap);
                                if (safeImage) {
                                    safeImage->setPixmap(pixmap);
                                    if (safeStack) safeStack->setCurrentWidget(safeImage);
                                    safeRenderer->hide();
                                    safeRenderer->deleteLater();
                                }
                                auto &waiting = GltfPreviewWidget::pendingThumbnails();
                                const auto labels = waiting.take(thumbnailKey);
                                for (const auto &label : labels)
                                    if (label) label->setPixmap(pixmap);
                            };
                            QTimer::singleShot(50, renderer, [capture]() { (*capture)(0); });
                        }, Qt::SingleShotConnection);
                    renderer->loadModel(modelPath, ktxPath);
                }
            } else {
                auto *placeholder = new QLabel(card);
                placeholder->setFixedSize(132, 88);
                placeholder->setAlignment(Qt::AlignCenter);
                placeholder->setPixmap(consistPhotoPlaceholder.pixmap(QSize(132, 88)));
                placeholder->setStyleSheet("background:#202a35; border:1px solid #52657d; border-radius:5px;");
                cardLayout->addWidget(placeholder, 0, Qt::AlignHCenter);
            }

            auto *nameLabel = new QLabel(unitName, card);
            nameLabel->setFixedWidth(138);
            nameLabel->setFixedHeight(34);
            nameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            nameLabel->setWordWrap(true);
            nameLabel->setStyleSheet("background:transparent; color:#e7eaf0; border:0; padding:0 2px;");
            cardLayout->addWidget(nameLabel, 0, Qt::AlignHCenter);
            consistStrip->setItemWidget(item, card);
        }
        consistStrip->updateCardWidths();
    };
    consistLayout->addWidget(consistStrip, 1);
    lowerTrains->addWidget(consistPanel, 76);

    auto [consistInfoPanel, consistInfoLayout] = makeTrainPanel("Информация о составе");
    auto *consistInfo = new QLabel("Тип состава: —\nКоличество единиц: 0\nОбщая длина: —\nОбщая масса: —");
    consistInfo->setWordWrap(true);
    consistInfo->setStyleSheet("QLabel { background:transparent; color:#cbd5e1; padding:8px; }");
    consistInfoLayout->addWidget(consistInfo, 1);
    lowerTrains->addWidget(consistInfoPanel, 24);
    trainsRoot->addLayout(lowerTrains, 2);

    auto *trainActionsPanel = makePanel();
    auto *trainActions = new QHBoxLayout(trainActionsPanel);
    trainActions->setContentsMargins(10, 6, 10, 6);
    trainActions->setSpacing(12);
    trainActions->addStretch(1);
    auto *assembleTrainButton = new QPushButton("⛓  Собрать новый поезд");
    auto *editTrainButton = new QPushButton("✓  Сохранить изменения поезда");
    auto *renameTrainButton = new QPushButton("✎  Изменить параметры поезда");
    auto *deleteTrainButton = new QPushButton("▤  Удалить поезд");
    for (auto *button : {assembleTrainButton, editTrainButton, renameTrainButton, deleteTrainButton}) {
        button->setObjectName("scenarioActionButton");
        button->setStyleSheet(scenarioActionButtonStyle);
        button->setMinimumWidth(175);
        button->setMinimumHeight(36);
        button->setCursor(Qt::PointingHandCursor);
        trainActions->addWidget(button);
    }
    trainActions->addStretch(1);
    trainsRoot->addWidget(trainActionsPanel);

    // Каталог поездов читается из cfg/trains. Файлы симулятора не изменяются.
    const QString trainsDirectory = QDir(simulatorRoot).filePath("cfg/trains");
    QDir trainDir(trainsDirectory);
    const QStringList trainFiles = trainDir.entryList({"*.xml"}, QDir::Files, QDir::Name);
    for (const QString &fileName : trainFiles) {
        QFile file(trainDir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QXmlStreamReader xml(&file);
        QString title, description, currentElement;
        double chargingPressure = 0.5, initMainResPressure = 0.9;
        bool noAir = false;
        QVariantList consistBlocks;
        QVariantMap currentVehicle;
        bool inCommon = false, inVehicle = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                currentElement = xml.name().toString();
                if (currentElement == "Common") inCommon = true;
                if (currentElement == "Vehicle") {
                    inVehicle = true; currentVehicle.clear();
                    currentVehicle["Count"] = 1;
                    currentVehicle["PayloadCoeff"] = "1";
                    currentVehicle["IsOrientationForward"] = "1";
                    currentVehicle["IsBrakeShoes"] = "0";
                }
            } else if (xml.isCharacters() && !xml.isWhitespace()) {
                const QString value = xml.text().toString().trimmed();
                if (inCommon && currentElement == "Title") title = value;
                else if (inCommon && currentElement == "Description") description = value;
                else if (inCommon && currentElement == "ChargingPressure") chargingPressure = value.toDouble();
                else if (inCommon && currentElement == "InitMainResPressure") initMainResPressure = value.toDouble();
                else if (inCommon && currentElement == "NoAir") noAir = (value == "1" || value.compare("true", Qt::CaseInsensitive) == 0);
                else if (inVehicle) currentVehicle[currentElement] = value;
            } else if (xml.isEndElement()) {
                const QString ended = xml.name().toString();
                if (ended == "Vehicle") {
                    if (!currentVehicle.value("ModuleConfig").toString().isEmpty()) {
                        currentVehicle["Count"] = qMax(1, currentVehicle.value("Count", 1).toInt());
                        consistBlocks << currentVehicle;
                    }
                    inVehicle = false;
                } else if (ended == "Common") inCommon = false;
                currentElement.clear();
            }
        }
        if (xml.hasError() || title.isEmpty()) continue;
        auto *item = new QListWidgetItem(title);
        QVariantMap data;
        data["name"] = title; data["description"] = description;
        data["chargingPressure"] = chargingPressure;
        data["initMainResPressure"] = initMainResPressure;
        data["noAir"] = noAir;
        data["sourceFile"] = trainDir.filePath(fileName);
        data["fileName"] = QFileInfo(fileName).completeBaseName();
        item->setData(Qt::UserRole, data);
        item->setData(Qt::UserRole + 1, consistBlocks);
        createdTrains->addItem(item);
    }
    if (createdTrains->count() == 0) {
        QMessageBox::warning(nullptr, "Поезда не найдены",
                             "В папке cfg/trains не найдено корректных XML-файлов поездов.");
    }

    // Каталог подвижных единиц читается из cfg/vehicles рекурсивно.
    // Название для отображения берётся из имени XML-конфига; файлы симулятора не изменяются.
    const QString vehiclesDirectory = QDir(simulatorRoot).filePath("cfg/vehicles");
    QDirIterator vehicleIt(vehiclesDirectory, {"*.xml"}, QDir::Files, QDirIterator::Subdirectories);
    while (vehicleIt.hasNext()) {
        const QString configPath = vehicleIt.next();
        // В каталог не попадают конфиги устройств, тормозов и прочих модулей.
        if (!isVehicleConfigFile(configPath)) continue;
        const QFileInfo configInfo(configPath);
        const QString configName = configInfo.completeBaseName();
        if (configName.isEmpty()) continue;
        auto *unitItem = new QListWidgetItem(configName);
        QVariantMap unitData;
        unitData["configPath"] = configPath;
        unitData["configName"] = configName;
        unitData["folder"] = configInfo.dir().dirName();
        unitItem->setData(Qt::UserRole, unitData);
        units->addItem(unitItem);
    }

    auto updateConsistInfo = [consistStrip, consistInfo, createdTrains, simulatorRoot] {
        auto *trainItem = createdTrains->currentItem();
        const QVariantMap trainData = trainItem ? trainItem->data(Qt::UserRole).toMap() : QVariantMap();
        const QString trainName = trainData.value("name", trainItem ? trainItem->text() : QString("—")).toString();
        const QString chargingPressure = trainItem
            ? QString::number(trainData.value("chargingPressure", 0.5).toDouble(), 'f', 2) : QString("—");
        const QString initialPressure = trainItem
            ? QString::number(trainData.value("initMainResPressure", 0.9).toDouble(), 'f', 2) : QString("—");
        const QString airPresence = trainItem
            ? (trainData.value("noAir", false).toBool() ? QString("Нет") : QString("Есть"))
            : QString("—");

        int totalUnits = 0;
        int totalAxes = 0;
        double totalLength = 0.0;
        double totalMassKg = 0.0;
        bool allSpecsFound = true;
        bool allAxisSpecsFound = true;
        const QString vehiclesDirectory = QDir(simulatorRoot).filePath("cfg/vehicles");

        for (int i = 0; i < consistStrip->count(); ++i) {
            const QVariantMap block = consistStrip->item(i)->data(Qt::UserRole).toMap();
            const int count = qMax(1, block.value("Count", 1).toInt());
            totalUnits += count;

            const QString configName = block.value("ModuleConfig").toString();
            const QString configFolder = block.value("ModuleConfigDir").toString();
            QString configPath = QDir(vehiclesDirectory).filePath(
                QDir(configFolder).filePath(configName + ".xml"));

            if (!QFileInfo::exists(configPath)) {
                QDirIterator it(vehiclesDirectory, {configName + ".xml"}, QDir::Files,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString candidate = it.next();
                    if (QFileInfo(candidate).dir().dirName() == configFolder) {
                        configPath = candidate;
                        break;
                    }
                }
            }

            QFile configFile(configPath);
            if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                allSpecsFound = false;
                continue;
            }

            QXmlStreamReader xml(&configFile);
            double emptyMass = -1.0;
            double payloadMass = 0.0;
            double unitLength = -1.0;
            int unitAxes = -1;
            while (!xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) continue;
                const QString tag = xml.name().toString();
                if (tag == QStringLiteral("EmptyMass"))
                    emptyMass = xml.readElementText().trimmed().toDouble();
                else if (tag == QStringLiteral("PayloadMass"))
                    payloadMass = xml.readElementText().trimmed().toDouble();
                else if (tag == QStringLiteral("Length"))
                    unitLength = xml.readElementText().trimmed().toDouble();
                else if (tag == QStringLiteral("NumAxis"))
                    unitAxes = xml.readElementText().trimmed().toInt();
            }

            if (xml.hasError() || emptyMass < 0.0 || unitLength < 0.0) {
                allSpecsFound = false;
                continue;
            }

            const bool isEmpty = block.value("PayloadCoeff", "1").toString() == "0";
            const double unitMass = emptyMass + (isEmpty ? 0.0 : payloadMass);
            totalLength += unitLength * count;
            totalMassKg += unitMass * count;
            if (unitAxes >= 0)
                totalAxes += unitAxes * count;
            else
                allAxisSpecsFound = false;
        }

        const QString axesText = allAxisSpecsFound
            ? QString::number(totalAxes)
            : QStringLiteral("нет данных");
        const QString lengthText = allSpecsFound
            ? QString::number(totalLength, 'f', 1) + QStringLiteral(" м")
            : QStringLiteral("нет данных");
        const QString massText = allSpecsFound
            ? QString::number(totalMassKg / 1000.0, 'f', 1) + QStringLiteral(" т")
            : QStringLiteral("нет данных");

        consistInfo->setText(
            QString("Имя поезда: %1\nКоличество подвижных единиц: %2\nКоличество осей: %3\nДлина поезда: %4\nМасса поезда: %5\nЗарядное давление: %6\nНачальное давление: %7\nНаличие воздуха: %8")
                .arg(trainName)
                .arg(totalUnits)
                .arg(axesText)
                .arg(lengthText)
                .arg(massText)
                .arg(chargingPressure)
                .arg(initialPressure)
                .arg(airPresence));
    };
    auto setConsistFromItem = [consistStrip, updateConsistInfo, addConsistUnit](QListWidgetItem *item) {
        if (!item) return;
        consistStrip->clear();
        consistStrip->updateCardWidths();
        const QVariantList savedBlocks = item->data(Qt::UserRole + 1).toList();
        for (const QVariant &block : savedBlocks) addConsistUnit(block.toMap());
        updateConsistInfo();
    };

    createdTrains->setCurrentRow(createdTrains->count() > 0 ? 0 : -1);
    units->setCurrentRow(units->count() > 0 ? 0 : -1);
    
    setUnitSpecs("Подвижная единица", "—", "—", "—", "—", "—", "—");
    updateConsistInfo();

    auto updateUnitPreview = [unitPreview, unitDetailsTitle, setUnitSpecs, simulatorRoot](QListWidgetItem *item) {
        if (!item) return;
        const QVariantMap unitData = item->data(Qt::UserRole).toMap();
        const QString configName = unitData.value("configName", item->text()).toString();
        const QString configPath = unitData.value("configPath").toString();
        unitDetailsTitle->setText(configName);

        // Характеристики берём из конфига Vehicle, независимо от наличия 3D-модели.
        QString unitType = configName;
        QString unitMass = "—";
        QString unitPayloadMass = "—";
        QString unitLength = "—";
        QString unitAxes = "—";
        QString unitCouplings = "—";
        QString unitWheelDiameter = "—";
        QFile vehicleConfigFile(configPath);
        if (vehicleConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QXmlStreamReader vehicleXml(&vehicleConfigFile);
            QString parsedType;
            QString parsedMass;
            QString parsedPayloadMass;
            QString parsedLength;
            QString parsedAxes;
            QString parsedCouplings;
            QString parsedCouplingModule;
            QString parsedWheelDiameter;
            while (!vehicleXml.atEnd()) {
                vehicleXml.readNext();
                if (!vehicleXml.isStartElement()) continue;
                const QString tag = vehicleXml.name().toString();
                if (tag == QStringLiteral("Type") ||
                    tag == QStringLiteral("EmptyMass") ||
                    tag == QStringLiteral("PayloadMass") ||
                    tag == QStringLiteral("Length") ||
                    tag == QStringLiteral("NumAxis") ||
                    tag == QStringLiteral("CouplingConfig") ||
                    tag == QStringLiteral("CouplingModule") ||
                    tag == QStringLiteral("WheelDiameter")) {
                    const QString value = vehicleXml.readElementText().trimmed();
                    if (tag == QStringLiteral("Type")) parsedType = value;
                    else if (tag == QStringLiteral("EmptyMass")) parsedMass = value;
                    else if (tag == QStringLiteral("PayloadMass")) parsedPayloadMass = value;
                    else if (tag == QStringLiteral("Length")) parsedLength = value;
                    else if (tag == QStringLiteral("NumAxis")) parsedAxes = value;
                    else if (tag == QStringLiteral("CouplingConfig")) parsedCouplings = value;
                    else if (tag == QStringLiteral("CouplingModule")) parsedCouplingModule = value;
                    else if (tag == QStringLiteral("WheelDiameter")) parsedWheelDiameter = value;
                }
            }
            if (!vehicleXml.hasError()) {
                if (!parsedType.isEmpty()) unitType = parsedType;
                bool ok = false;
                const double massKg = parsedMass.toDouble(&ok);
                if (ok) unitMass = QString::number(massKg / 1000.0, 'f', 1) + " т";
                const double payloadKg = parsedPayloadMass.toDouble(&ok);
                if (ok) unitPayloadMass = QString::number(payloadKg / 1000.0, 'f', 1) + " т";
                const double lengthMeters = parsedLength.toDouble(&ok);
                if (ok) unitLength = QString::number(lengthMeters, 'f', 1) + " м";
                const int axesCount = parsedAxes.toInt(&ok);
                if (ok) unitAxes = QString::number(axesCount);
                if (!parsedCouplingModule.isEmpty() && !parsedCouplings.isEmpty())
                    unitCouplings = parsedCouplingModule + " / " + parsedCouplings;
                else if (!parsedCouplingModule.isEmpty()) unitCouplings = parsedCouplingModule;
                else if (!parsedCouplings.isEmpty()) unitCouplings = parsedCouplings;
                const double wheelMeters = parsedWheelDiameter.toDouble(&ok);
                if (ok) unitWheelDiameter = QString::number(wheelMeters, 'f', 3) + " м";
            }
        }
        setUnitSpecs(unitType, unitMass, unitPayloadMass, unitLength,
                     unitAxes, unitCouplings, unitWheelDiameter);

        // ModelName трактуется относительно data/models, как в структуре RRS.
        // Используем только первый непосредственный блок Model; модель кабины игнорируется.
        const QString modelName = readPrimaryModelName(configPath);

        if (modelName.isEmpty()) {
            unitPreview->loadModel(QString());
            unitPreview->setStatusText(QString("Не найден ModelName. Конфиг: %1").arg(QFileInfo(configPath).fileName()));
            return;
        }

        // Например, VL60pk-1543/VL60pk-1543.gltf превращается в:
        // <simulatorRoot>/data/models/VL60pk-1543/VL60pk-1543.gltf.
        const QString modelsRoot = QDir(simulatorRoot).filePath("data/models");
        // ModelName в конфиге — относительный путь внутри data/models.
        // Нормализуем разделители перед сборкой абсолютного пути (особенно важно для Windows).
        QString relativeModelName = QDir::fromNativeSeparators(modelName.trimmed());
        while (relativeModelName.startsWith("./"))
            relativeModelName.remove(0, 2);
        if (QDir::isAbsolutePath(relativeModelName) ||
            QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(relativeModelName).hasMatch()) {
            unitPreview->loadModel(QString());
            unitPreview->setStatusText(QString("ModelName должен быть относительным путём: %1").arg(modelName));
            return;
        }
        const QString modelPath = QDir::cleanPath(QDir(modelsRoot).absoluteFilePath(relativeModelName));
        if (!QFileInfo::exists(modelPath)) {
            unitPreview->loadModel(QString());
            unitPreview->setStatusText(QString("GLTF-файл не найден. Проверяемый путь:\n%1\nModelName: %2").arg(modelPath, modelName));
            return;
        }

        QFile gltfFile(modelPath);
        if (!gltfFile.open(QIODevice::ReadOnly)) {
            unitPreview->loadModel(QString());
            unitPreview->setStatusText(QString("Файл найден, но не открывается:\n%1\n%2").arg(modelPath, gltfFile.errorString()));
            return;
        }
        QJsonParseError jsonError{};
        const QJsonDocument gltf = QJsonDocument::fromJson(gltfFile.readAll(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError || !gltf.isObject()) {
            unitPreview->loadModel(QString());
            unitPreview->setStatusText(QString("Ошибка разбора GLTF:\n%1\n%2").arg(modelPath, jsonError.errorString()));
            return;
        }

        const QJsonObject root = gltf.object();
        const QJsonArray meshes = root.value("meshes").toArray();
        const QJsonArray nodes = root.value("nodes").toArray();
        const QJsonArray animations = root.value("animations").toArray();
        const QJsonArray buffers = root.value("buffers").toArray();
        QString bufferStatus = "не требуется";
        qint64 bufferBytes = 0;
        if (!buffers.isEmpty()) {
            const QString bufferUri = buffers.first().toObject().value("uri").toString();
            if (!bufferUri.isEmpty()) {
                const QString bufferPath = QFileInfo(modelPath).dir().filePath(bufferUri);
                QFile bufferFile(bufferPath);
                if (bufferFile.open(QIODevice::ReadOnly)) {
                    bufferBytes = bufferFile.size();
                    bufferStatus = "прочитан";
                } else {
                    bufferStatus = "не найден / недоступен";
                }
            }
        }
        const QJsonArray images = root.value("images").toArray();
        int textureCount = images.size();
        int textureFilesFound = 0;
        for (const QJsonValue &imageValue : images) {
            const QString imageUri = imageValue.toObject().value("uri").toString();
            if (!imageUri.isEmpty() && QFileInfo::exists(QFileInfo(modelPath).dir().filePath(imageUri)))
                ++textureFilesFound;
        }
        unitPreview->loadModel(modelPath, QDir(simulatorRoot).filePath("bin/ktx.exe"));
    };
    auto updateConsistUnitPreview = [updateUnitPreview, simulatorRoot, emptyLoadCheck,
                                     reverseOrientationCheck, brakeShoesCheck](QListWidgetItem *item) {
        if (!item) return;
        const QVariantMap block = item->data(Qt::UserRole).toMap();
        const QString configName = block.value("ModuleConfig").toString();
        const QString configFolder = block.value("ModuleConfigDir").toString();
        const QString vehiclesDirectory = QDir(simulatorRoot).filePath("cfg/vehicles");

        QString configPath = QDir(vehiclesDirectory).filePath(
            QDir(configFolder).filePath(configName + ".xml"));
        if (!QFileInfo::exists(configPath)) {
            // Резервный поиск учитывает конфиги, расположенные во вложенных каталогах.
            QDirIterator it(vehiclesDirectory, {configName + ".xml"}, QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (QFileInfo(candidate).dir().dirName() == configFolder) {
                    configPath = candidate;
                    break;
                }
            }
        }

        QListWidgetItem previewItem(configName);
        QVariantMap previewData;
        previewData["configName"] = configName;
        previewData["configPath"] = configPath;
        previewItem.setData(Qt::UserRole, previewData);
        updateUnitPreview(&previewItem);

        const QSignalBlocker emptyBlocker(emptyLoadCheck);
        const QSignalBlocker orientationBlocker(reverseOrientationCheck);
        const QSignalBlocker shoesBlocker(brakeShoesCheck);
        emptyLoadCheck->setChecked(block.value("PayloadCoeff", "1").toString() == "0");
        reverseOrientationCheck->setChecked(block.value("IsOrientationForward", "1").toString() == "0");
        brakeShoesCheck->setChecked(block.value("IsBrakeShoes", "0").toString() == "1");
    };
    QObject::connect(consistStrip, &QListWidget::itemClicked, trainsPage,
        [updateConsistUnitPreview](QListWidgetItem *item) {
            if (item) updateConsistUnitPreview(item);
        });

    QObject::connect(units, &QListWidget::currentItemChanged, trainsPage, [updateUnitPreview](QListWidgetItem *current) { updateUnitPreview(current); });
    updateUnitPreview(units->currentItem());
    QObject::connect(unitSearch, &QLineEdit::textChanged, trainsPage, [units](const QString &text) {
        for (int i = 0; i < units->count(); ++i) units->item(i)->setHidden(!units->item(i)->text().contains(text, Qt::CaseInsensitive));
    });
    QObject::connect(createdTrainSearch, &QLineEdit::textChanged, trainsPage, [createdTrains](const QString &text) {
        for (int i = 0; i < createdTrains->count(); ++i) createdTrains->item(i)->setHidden(!createdTrains->item(i)->text().contains(text, Qt::CaseInsensitive));
    });
    auto *loadedTrain = new QListWidgetItem *(createdTrains->currentItem());
    auto *consistDirty = new bool(false);
    QObject::connect(addUnitButton, &QPushButton::clicked, trainsPage,
        [units, consistStrip, consistDirty, addConsistUnit, emptyLoadCheck, reverseOrientationCheck, brakeShoesCheck, updateConsistInfo] {
            auto *selected = units->currentItem();
            if (!selected) { QMessageBox::information(nullptr, "Добавление единицы", "Выберите подвижную единицу в каталоге."); return; }
            const QVariantMap unitData = selected->data(Qt::UserRole).toMap();
            const QString configName = unitData.value("configName", selected->text()).toString();
            QVariantMap block;
            block["ModuleConfigDir"] = unitData.value("folder");
            block["ModuleConfig"] = configName;
            block["IsOrientationForward"] = reverseOrientationCheck->isChecked() ? "0" : "1";
            block["PayloadCoeff"] = emptyLoadCheck->isChecked() ? "0" : "1";
            block["Count"] = 1;
            block["IsBrakeShoes"] = brakeShoesCheck->isChecked() ? "1" : "0";
            addConsistUnit(block);
            consistStrip->updateCardWidths();
            *consistDirty = true;
            updateConsistInfo();
        });
    QObject::connect(removeUnitButton, &QPushButton::clicked, trainsPage, [consistStrip, consistDirty, updateConsistInfo] {
        auto *selected = consistStrip->currentItem();
        if (!selected) { QMessageBox::information(nullptr, "Удаление единицы", "Выберите единицу в ленте состава."); return; }
        delete consistStrip->takeItem(consistStrip->row(selected));
        consistStrip->updateCardWidths();
        *consistDirty = true;
        updateConsistInfo();
    });
    auto showTrainDialog = [trainsPage](const QString &title, const QString &initialName,
                                        const QString &initialFileName, const QString &initialDescription,
                                        double initialChargingPressure, double initialInitMainResPressure,
                                        bool initialNoAir, QString *nameOut, QString *fileNameOut,
                                        QString *descriptionOut, double *chargingPressureOut,
                                        double *initMainResPressureOut, bool *noAirOut,
                                        const QString &acceptText) {
        QDialog dialog(trainsPage);
        dialog.setWindowTitle(title);
        dialog.setModal(true);
        dialog.setMinimumWidth(520);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(10);
        auto *form = new QFormLayout;
        form->setSpacing(8);
        auto *nameEdit = new QLineEdit(initialName);
        nameEdit->setPlaceholderText("Введите название поезда...");
        nameEdit->setMinimumHeight(36);
        auto *fileNameEdit = new QLineEdit(initialFileName);
        fileNameEdit->setPlaceholderText("Например: MyPassengerTrain");
        fileNameEdit->setMinimumHeight(36);
        auto *fileNameHint = new QLabel("Только английские буквы A–Z, цифры, дефис и подчёркивание. Без .xml");
        fileNameHint->setWordWrap(true);
        fileNameHint->setStyleSheet("color:#9eabb9; font-size:11px;");
        auto *descriptionEdit = new QTextEdit;
        descriptionEdit->setPlaceholderText("Введите описание поезда...");
        descriptionEdit->setPlainText(initialDescription);
        descriptionEdit->setMinimumHeight(110);
        auto *chargingPressureEdit = new QDoubleSpinBox;
        chargingPressureEdit->setRange(0.0, 10.0);
        chargingPressureEdit->setDecimals(2);
        chargingPressureEdit->setSingleStep(0.05);
        chargingPressureEdit->setValue(initialChargingPressure);
        chargingPressureEdit->setMinimumHeight(34);
        auto *initMainResPressureEdit = new QDoubleSpinBox;
        initMainResPressureEdit->setRange(0.0, 10.0);
        initMainResPressureEdit->setDecimals(2);
        initMainResPressureEdit->setSingleStep(0.05);
        initMainResPressureEdit->setValue(initialInitMainResPressure);
        initMainResPressureEdit->setMinimumHeight(34);
        auto *noAirCheck = new GraphicsCheckBox("Поезд без воздуха");
        noAirCheck->setStyleSheet(R"(
            QCheckBox { background:transparent; color:#e7edf5; spacing:8px; }
            QCheckBox::indicator {
                width:16px; height:16px;
                border:1px solid #718ba5;
                border-radius:4px;
                background:#202c39;
            }
            QCheckBox::indicator:checked {
                background:#202c39;
                border:1px solid #75baf0;
            }
            QCheckBox::indicator:unchecked:hover {
                border:1px solid #9bcdf4;
                background:#30465c;
            }
        )");
        noAirCheck->setCursor(Qt::PointingHandCursor);
        noAirCheck->setChecked(initialNoAir);
        form->addRow("Имя файла (латиница):", fileNameEdit);
        form->addRow("", fileNameHint);
        form->addRow("Название поезда:", nameEdit);
        form->addRow("Зарядное давление:", chargingPressureEdit);
        form->addRow("Начальное давление:", initMainResPressureEdit);
        form->addRow("", noAirCheck);
        form->addRow("Описание:", descriptionEdit);
        layout->addLayout(form);
        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *accept = new QPushButton(acceptText);
        accept->setObjectName("primary");
        auto *cancel = new QPushButton("Отмена");
        buttons->addWidget(accept);
        buttons->addWidget(cancel);
        layout->addLayout(buttons);
        QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        QObject::connect(accept, &QPushButton::clicked, &dialog, [&dialog, nameEdit, fileNameEdit] {
            if (nameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(&dialog, "Название не задано", "Введите название поезда.");
                return;
            }
            const QString fileName = fileNameEdit->text().trimmed();
            if (!isValidTrainFileName(fileName)) {
                QMessageBox::warning(&dialog, "Некорректное имя файла",
                    "Имя файла обязательно должно быть написано латинскими буквами.\n"
                    "Разрешены только A–Z, a–z, цифры, дефис и подчёркивание.\n"
                    "Расширение .xml добавляется автоматически.");
                fileNameEdit->setFocus();
                fileNameEdit->selectAll();
                return;
            }
            dialog.accept();
        });
        if (dialog.exec() != QDialog::Accepted) return false;
        *nameOut = nameEdit->text().trimmed();
        *fileNameOut = fileNameEdit->text().trimmed();
        *descriptionOut = descriptionEdit->toPlainText().trimmed();
        *chargingPressureOut = chargingPressureEdit->value();
        *initMainResPressureOut = initMainResPressureEdit->value();
        *noAirOut = noAirCheck->isChecked();
        return true;
    };

    QObject::connect(assembleTrainButton, &QPushButton::clicked, trainsPage,
        [trainsPage, showTrainDialog, createdTrains, consistStrip, updateConsistInfo, loadedTrain, consistDirty] {
            QString name, fileName, description;
            double chargingPressure = 0.5, initMainResPressure = 0.9;
            bool noAir = false;
            if (!showTrainDialog("Сборка нового поезда", "", "", "", 0.5, 0.9, false,
                                 &name, &fileName, &description, &chargingPressure,
                                 &initMainResPressure, &noAir, "Подготовить поезд")) return;

            // Создаём только временную запись в интерфейсе. XML появится
            // исключительно после нажатия «Сохранить изменения поезда».
            auto *item = new QListWidgetItem(name);
            QVariantMap data;
            data["name"] = name;
            data["fileName"] = fileName;
            data["sourceFile"] = QString();
            data["description"] = description;
            data["chargingPressure"] = chargingPressure;
            data["initMainResPressure"] = initMainResPressure;
            data["noAir"] = noAir;
            item->setData(Qt::UserRole, data);
            item->setData(Qt::UserRole + 1, QVariantList());
            createdTrains->addItem(item);
            *loadedTrain = item;
            *consistDirty = false;
            consistStrip->clear();
            consistStrip->updateCardWidths();
            createdTrains->setCurrentItem(item);
            updateConsistInfo();
        });

    QObject::connect(editTrainButton, &QPushButton::clicked, trainsPage,
        [trainsPage, createdTrains, consistStrip, consistDirty, loadedTrain, updateConsistInfo, trainsDirectory] {
            auto *selected = createdTrains->currentItem();
            if (!selected) {
                QMessageBox::information(trainsPage, "Сохранение состава", "Выберите поезд в списке.");
                return;
            }
            QVariantList blocks;
            for (int i = 0; i < consistStrip->count(); ++i)
                blocks << consistStrip->item(i)->data(Qt::UserRole);
            QVariantMap trainData = selected->data(Qt::UserRole).toMap();
            QString fileName = trainData.value("fileName").toString();
            if (!isValidTrainFileName(fileName)) {
                QMessageBox::warning(trainsPage, "Имя файла не задано",
                    "Откройте «Изменить имя поезда» и укажите имя XML-файла латиницей.");
                return;
            }
            const QString targetPath = QDir(trainsDirectory).filePath(fileName + ".xml");
            const QString oldPath = trainData.value("sourceFile").toString();
            if (QFileInfo::exists(targetPath) && QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath()) != QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath())) {
                QMessageBox::warning(trainsPage, "Файл уже существует",
                    QString("Невозможно сохранить поезд: %1.xml уже существует.").arg(fileName));
                return;
            }
            if (!QDir().mkpath(trainsDirectory)) {
                QMessageBox::critical(trainsPage, "Ошибка сохранения",
                    QString("Не удалось создать или открыть папку конфигураций:\n%1").arg(trainsDirectory));
                return;
            }
            QString error;
            if (!saveTrainConfig(targetPath, trainData.value("name", selected->text()).toString(),
                                 trainData.value("description").toString(),
                                 trainData.value("chargingPressure", 0.5).toDouble(),
                                 trainData.value("initMainResPressure", 0.9).toDouble(),
                                 trainData.value("noAir", false).toBool(), blocks, &error)) {
                QMessageBox::critical(trainsPage, "Ошибка сохранения", error);
                return;
            }
            if (!oldPath.isEmpty() && QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath()) !=
                QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath()) && QFileInfo::exists(oldPath)) {
                if (!QFile::remove(oldPath)) {
                    QMessageBox::warning(trainsPage, "Старый файл не удалён",
                        QString("Новая конфигурация сохранена, но старый файл остался:\n%1").arg(oldPath));
                }
            }
            trainData["sourceFile"] = targetPath;
            selected->setData(Qt::UserRole, trainData);
            selected->setData(Qt::UserRole + 1, blocks);
            *loadedTrain = selected;
            *consistDirty = false;
            updateConsistInfo();
            QMessageBox::information(trainsPage, "Сохранение состава",
                QString("Конфигурация поезда сохранена:\n%1").arg(targetPath));
        });

    QObject::connect(renameTrainButton, &QPushButton::clicked, trainsPage,
        [trainsPage, createdTrains, consistStrip, showTrainDialog, consistDirty,
         loadedTrain, updateConsistInfo, trainsDirectory] {
            auto *selected = createdTrains->currentItem();
            if (!selected) {
                QMessageBox::information(trainsPage, "Изменение поезда", "Выберите поезд в списке.");
                return;
            }
            const QVariantMap currentData = selected->data(Qt::UserRole).toMap();
            QString name, fileName, description;
            double chargingPressure = currentData.value("chargingPressure", 0.5).toDouble();
            double initMainResPressure = currentData.value("initMainResPressure", 0.9).toDouble();
            bool noAir = currentData.value("noAir", false).toBool();
            if (!showTrainDialog("Изменение имени и параметров поезда",
                                 currentData.value("name", selected->text()).toString(),
                                 currentData.value("fileName").toString(),
                                 currentData.value("description").toString(),
                                 chargingPressure, initMainResPressure, noAir,
                                 &name, &fileName, &description, &chargingPressure,
                                 &initMainResPressure, &noAir, "Применить"))
                return;

            if (!isValidTrainFileName(fileName)) {
                QMessageBox::warning(trainsPage, "Некорректное имя файла",
                    "Имя файла обязательно должно быть написано латинскими буквами.");
                return;
            }

            QVariantList blocks;
            for (int i = 0; i < consistStrip->count(); ++i)
                blocks << consistStrip->item(i)->data(Qt::UserRole);

            QVariantMap updatedData = currentData;
            updatedData["name"] = name;
            updatedData["fileName"] = fileName;
            updatedData["description"] = description;
            updatedData["chargingPressure"] = chargingPressure;
            updatedData["initMainResPressure"] = initMainResPressure;
            updatedData["noAir"] = noAir;

            const QString targetPath = QDir(trainsDirectory).filePath(fileName + ".xml");
            const QString oldPath = currentData.value("sourceFile").toString();
            const QString cleanTarget = QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
            const QString cleanOld = oldPath.isEmpty() ? QString() :
                QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath());
            if (QFileInfo::exists(targetPath) && cleanTarget != cleanOld) {
                QMessageBox::warning(trainsPage, "Файл уже существует",
                    QString("Невозможно применить изменения: файл %1.xml уже существует.").arg(fileName));
                return;
            }
            if (!QDir().mkpath(trainsDirectory)) {
                QMessageBox::critical(trainsPage, "Ошибка сохранения",
                    QString("Не удалось создать или открыть папку конфигураций:\n%1").arg(trainsDirectory));
                return;
            }

            QString error;
            if (!saveTrainConfig(targetPath, name, description, chargingPressure,
                                 initMainResPressure, noAir, blocks, &error)) {
                QMessageBox::critical(trainsPage, "Ошибка сохранения",
                    QString("Изменения не применены: не удалось сохранить XML-файл.\n\n%1").arg(error));
                return;
            }

            // Если изменилось имя XML-файла, удаляем старый файл только после
            // успешной записи нового.
            if (!oldPath.isEmpty() && cleanOld != cleanTarget && QFileInfo::exists(oldPath)) {
                if (!QFile::remove(oldPath)) {
                    QMessageBox::warning(trainsPage, "Старый файл не удалён",
                        QString("Новая конфигурация сохранена, но старый файл остался:\n%1").arg(oldPath));
                }
            }

            updatedData["sourceFile"] = targetPath;
            {
                const QSignalBlocker blockListSignals(createdTrains);
                selected->setData(Qt::UserRole, updatedData);
                selected->setText(name);
                selected->setData(Qt::UserRole + 1, blocks);
            }
            *loadedTrain = selected;
            *consistDirty = false;
            updateConsistInfo();
            QMessageBox::information(trainsPage, "Изменение поезда",
                QString("Изменения применены и сохранены:\n%1").arg(targetPath));
        });

    QObject::connect(deleteTrainButton, &QPushButton::clicked, trainsPage,
        [trainsPage, createdTrains, consistStrip, consistInfo, loadedTrain, consistDirty, trainsDirectory] {
            auto *selected = createdTrains->currentItem();
            if (!selected) {
                QMessageBox::information(trainsPage, "Удаление поезда", "Выберите поезд для удаления.");
                return;
            }

            const QVariantMap trainData = selected->data(Qt::UserRole).toMap();
            const QString configPath = trainData.value("sourceFile").toString();
            QString confirmText = QString("Удалить поезд «%1»?").arg(selected->text());
            if (!configPath.isEmpty() && QFileInfo::exists(configPath))
                confirmText += QString("\\n\\nТакже будет удалён файл конфигурации:\\n%1").arg(configPath);

            if (QMessageBox::question(trainsPage, "Удаление поезда", confirmText,
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                return;

            // Удаляем XML только у уже сохранённого поезда. Для новой,
            // ещё не сохранённой заготовки sourceFile пустой.
            if (!configPath.isEmpty() && QFileInfo::exists(configPath)) {
                const QString configDir = QDir::cleanPath(QFileInfo(configPath).absolutePath());
                const QString expectedDir = QDir::cleanPath(QFileInfo(trainsDirectory).absoluteFilePath());
                if (configDir.compare(expectedDir, Qt::CaseInsensitive) != 0) {
                    QMessageBox::critical(trainsPage, "Удаление отменено",
                        QString("Файл конфигурации находится вне папки поездов и не будет удалён:\\n%1").arg(configPath));
                    return;
                }
                if (!QFile::remove(configPath)) {
                    QMessageBox::critical(trainsPage, "Ошибка удаления файла",
                        QString("Не удалось удалить конфигурацию поезда:\\n%1\\n\\nПоезд оставлен в списке.").arg(configPath));
                    return;
                }
            }

            const bool deletingLoadedTrain = (*loadedTrain == selected);
            if (deletingLoadedTrain) {
                *loadedTrain = nullptr;
                *consistDirty = false;
                consistStrip->clear();
                consistStrip->updateCardWidths();
            }
            delete createdTrains->takeItem(createdTrains->row(selected));
            if (createdTrains->count() == 0) {
                *loadedTrain = nullptr;
                *consistDirty = false;
                consistStrip->clear();
                consistStrip->updateCardWidths();
                consistInfo->setText("Тип состава: —\\nКоличество единиц: 0\\nОбщая длина: —\\nОбщая масса: —");
            }
        });

    QObject::connect(createdTrains, &QListWidget::currentItemChanged, trainsPage,
        [trainsPage, createdTrains, consistStrip, consistInfo, loadedTrain, consistDirty,
         updateConsistInfo, addConsistUnit, trainsDirectory]
        (QListWidgetItem *current, QListWidgetItem *) {
            if (!current || current == *loadedTrain) return;
            if (*consistDirty && *loadedTrain) {
                QListWidgetItem *previous = *loadedTrain;
                QMessageBox prompt(trainsPage);
                prompt.setWindowTitle("Несохранённые изменения");
                prompt.setText(QString("В поезде «%1» есть несохранённые изменения. Сохранить их перед переключением?")
                               .arg(previous->text()));
                prompt.setIcon(QMessageBox::Question);
                auto *saveButton = prompt.addButton("Сохранить", QMessageBox::AcceptRole);
                auto *discardButton = prompt.addButton("Оставить без изменений", QMessageBox::DestructiveRole);
                prompt.setDefaultButton(qobject_cast<QPushButton *>(saveButton));
                prompt.exec();

                if (prompt.clickedButton() == saveButton) {
                    QVariantList blocks;
                    for (int i = 0; i < consistStrip->count(); ++i)
                        blocks << consistStrip->item(i)->data(Qt::UserRole);

                    QVariantMap trainData = previous->data(Qt::UserRole).toMap();
                    const QString fileName = trainData.value("fileName").toString();
                    if (!isValidTrainFileName(fileName)) {
                        QMessageBox::warning(trainsPage, "Имя файла не задано",
                            "Невозможно сохранить поезд: укажите корректное имя XML-файла латиницей через «Изменить имя поезда».");
                        createdTrains->setCurrentItem(previous);
                        return;
                    }

                    const QString targetPath = QDir(trainsDirectory).filePath(fileName + ".xml");
                    const QString oldPath = trainData.value("sourceFile").toString();
                    const QString cleanTarget = QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
                    const QString cleanOld = oldPath.isEmpty() ? QString() :
                        QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath());
                    if (QFileInfo::exists(targetPath) && cleanTarget != cleanOld) {
                        QMessageBox::warning(trainsPage, "Файл уже существует",
                            QString("Невозможно сохранить поезд: %1.xml уже существует.").arg(fileName));
                        createdTrains->setCurrentItem(previous);
                        return;
                    }
                    if (!QDir().mkpath(trainsDirectory)) {
                        QMessageBox::critical(trainsPage, "Ошибка сохранения",
                            QString("Не удалось создать или открыть папку конфигураций:\n%1").arg(trainsDirectory));
                        createdTrains->setCurrentItem(previous);
                        return;
                    }

                    QString error;
                    if (!saveTrainConfig(targetPath,
                            trainData.value("name", previous->text()).toString(),
                            trainData.value("description").toString(),
                            trainData.value("chargingPressure", 0.5).toDouble(),
                            trainData.value("initMainResPressure", 0.9).toDouble(),
                            trainData.value("noAir", false).toBool(), blocks, &error)) {
                        QMessageBox::critical(trainsPage, "Ошибка сохранения",
                            QString("Не удалось сохранить поезд:\n%1").arg(error));
                        createdTrains->setCurrentItem(previous);
                        return;
                    }

                    // Старый файл удаляем только после успешной записи нового.
                    if (!oldPath.isEmpty() && cleanOld != cleanTarget && QFileInfo::exists(oldPath)) {
                        if (!QFile::remove(oldPath)) {
                            QMessageBox::warning(trainsPage, "Старый файл не удалён",
                                QString("Новая конфигурация сохранена, но старый файл остался:\n%1").arg(oldPath));
                        }
                    }
                    trainData["sourceFile"] = targetPath;
                    previous->setData(Qt::UserRole, trainData);
                    previous->setData(Qt::UserRole + 1, blocks);
                    *consistDirty = false;
                } else if (prompt.clickedButton() == discardButton) {
                    // Не сохраняем изменения: при загрузке следующего поезда
                    // берём последний подтверждённый снимок состава.
                } else {
                    // Закрытие окна эквивалентно отмене переключения.
                    createdTrains->setCurrentItem(previous);
                    return;
                }
            }
            consistStrip->clear();
            consistStrip->updateCardWidths();
            const QVariantList savedBlocks = current->data(Qt::UserRole + 1).toList();
            for (const QVariant &block : savedBlocks) addConsistUnit(block.toMap());
            *loadedTrain = current;
            *consistDirty = false;
            updateConsistInfo();
        });
    if (createdTrains->currentItem()) {
        const QVariantList initialBlocks = createdTrains->currentItem()->data(Qt::UserRole + 1).toList();
        for (const QVariant &block : initialBlocks) addConsistUnit(block.toMap());
        updateConsistInfo();
        *loadedTrain = createdTrains->currentItem();
    }

    

    // Раздел дополнений — отдельная страница перед журналом, чтобы индексы
    // страниц соответствовали вкладкам sectionBar.
    auto *addonsPage = new QWidget;
    auto *addonsLayout = new QVBoxLayout(addonsPage);
    addonsLayout->setContentsMargins(8, 8, 8, 8);
    addonsLayout->setSpacing(12);
    auto *addonsPanel = makePanel();
    // Убираем тёмную подложку панели раздела «Дополнения».
    addonsPanel->setStyleSheet("QFrame#panel { background:transparent; border:0; }");
    auto *addonsPanelLayout = new QVBoxLayout(addonsPanel);
    addonsPanelLayout->addWidget(heading("Дополнения"));
    auto *addonsPlaceholder = new QLabel("Раздел находится в разработке.");
    addonsPlaceholder->setAlignment(Qt::AlignCenter);
    addonsPanelLayout->addWidget(addonsPlaceholder, 1);
    addonsLayout->addWidget(addonsPanel, 1);
    

    // Раздел журналов: выбор типа журнала через раскрывающийся список.
    auto *journalPage = new QWidget;
    journalPage->setStyleSheet("QWidget { background:transparent; }");
    auto *journalLayout = new QVBoxLayout(journalPage);
    journalLayout->setContentsMargins(8, 8, 8, 8);
    journalLayout->setSpacing(12);

    auto *journalSelectorRow = new QHBoxLayout;
    auto *journalSelector = new JournalMenuButton;
    journalSelector->setText("Журналы логирования");
    journalSelector->setToolButtonStyle(Qt::ToolButtonTextOnly);
    journalSelector->setPopupMode(QToolButton::InstantPopup);
    journalSelector->setMinimumHeight(38);
    journalSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    journalSelector->setStyleSheet("QToolButton { background:#343b46; border:1px solid #4b5563; border-radius:6px; padding:8px 30px 8px 13px; text-align:left; } QToolButton:hover { background:#414b59; border-color:#5b6675; } QToolButton:pressed { background:#3b4655; border-color:#5b6675; } QToolButton::menu-indicator { image:none; width:0px; }");
    auto *journalMenu = new QMenu(journalSelector);
    journalMenu->setStyleSheet(
        "QMenu { background:#343b46; color:#e7eaf0; border:1px solid #586779; "
        "border-radius:6px; padding:4px; } "
        "QMenu::item { background:transparent; padding:7px 12px; border-radius:4px; } "
        "QMenu::item:selected { background:#354b68; color:white; }");
    auto *journalDisplayLabel = new QLabel("Отображать");
    journalDisplayLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    auto *journalDisplayFilter = new MainSettingsComboBox;
    journalDisplayFilter->setFixedWidth(250);
    journalDisplayFilter->addItem("Всю информацию", QString());
    journalDisplayFilter->addItem("Информационные сообщения", "I");
    journalDisplayFilter->addItem("Предупреждения", "W");
    journalDisplayFilter->addItem("Ошибки", "E");
    journalDisplayFilter->addItem("Критические ошибки", "C");
    journalDisplayFilter->setMinimumHeight(38);
    journalDisplayFilter->setMinimumWidth(250);

    auto *journalContent = new QPlainTextEdit;
    journalContent->setReadOnly(true);
    journalContent->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    journalContent->setPlaceholderText("Содержимое TXT-файла выбранного журнала будет отображаться здесь.");
    journalContent->setPlainText("Содержимое TXT-файла выбранного журнала будет отображаться здесь.");
    journalContent->setStyleSheet(R"(
        QPlainTextEdit {
            background:#1f2329;
            color:#cbd5e1;
            border:1px solid #3a414c;
            border-radius:6px;
            padding:10px;
            selection-background-color:#354b68;
        }
        QPlainTextEdit:focus { border:1px solid #52657d; }
        QScrollBar:vertical { background:#252a31; width:10px; margin:2px; }
        QScrollBar::handle:vertical { background:#4b5665; border-radius:4px; min-height:24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )");
    journalSelector->setMenu(journalMenu);
    QString journalRawText;
    QString journalSelectedFilePath;
    const auto refreshJournalView = [journalContent, journalDisplayFilter, &journalRawText] {
        const QString prefix = journalDisplayFilter->currentData().toString();
        if (prefix.isEmpty()) {
            journalContent->setPlainText(journalRawText);
            return;
        }
        QStringList visibleLines;
        const QStringList lines = journalRawText.split(QRegularExpression("\\r?\\n"));
        for (const QString &line : lines) {
            if (line.startsWith(prefix))
                visibleLines.append(line);
        }
        journalContent->setPlainText(visibleLines.join("\n"));
    };
    const auto reloadSelectedJournal = [journalContent, &journalRawText, &journalSelectedFilePath, refreshJournalView] {
        if (journalSelectedFilePath.isEmpty())
            return;
        QFile logFile(journalSelectedFilePath);
        if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            journalRawText = "Не удалось обновить файл журнала: " + QFileInfo(journalSelectedFilePath).fileName()
                + "\n" + logFile.errorString();
            refreshJournalView();
            return;
        }
        QTextStream stream(&logFile);
        journalRawText = stream.readAll();
        refreshJournalView();
    };
    QObject::connect(journalDisplayFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     journalContent, [refreshJournalView](int) { refreshJournalView(); });
    QObject::connect(journalMenu, &QMenu::aboutToShow, journalSelector,
                     [journalMenu, journalSelector, journalContent, simulatorRoot, &journalRawText, &journalSelectedFilePath, refreshJournalView, reloadSelectedJournal] {
        journalMenu->clear();
        const QString logsDirectory = QDir(simulatorRoot).filePath("logs");
        QDir logsDir(logsDirectory);
        QFileInfoList logFiles = logsDir.entryInfoList(
            QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        // Файлы с '~' в имени являются временными/резервными и в списке журналов не показываются.
        for (auto it = logFiles.begin(); it != logFiles.end();) {
            if (it->fileName().contains('~'))
                it = logFiles.erase(it);
            else
                ++it;
        }

        if (logFiles.isEmpty()) {
            QAction *emptyAction = journalMenu->addAction(
                QDir(logsDirectory).exists() ? "В папке logs нет файлов" : "Папка logs не найдена");
            emptyAction->setEnabled(false);
        } else {
            for (const QFileInfo &logInfo : logFiles) {
                const QString filePath = logInfo.absoluteFilePath();
                const QString fileName = logInfo.fileName();
                journalMenu->addAction(fileName, journalSelector,
                    [journalSelector, fileName, filePath, &journalSelectedFilePath, reloadSelectedJournal] {
                        journalSelector->setText(fileName);
                        journalSelectedFilePath = filePath;
                        reloadSelectedJournal();
                    });
            }
        }
        // Подгоняем выпадающий список под фактическую ширину кнопки при каждом открытии.
        journalMenu->setFixedWidth(journalSelector->width());
    });
    journalSelectorRow->addWidget(journalSelector, 1);
    journalSelectorRow->addWidget(journalDisplayLabel);
    journalSelectorRow->addWidget(journalDisplayFilter);
    journalLayout->addLayout(journalSelectorRow);

    journalLayout->addWidget(journalContent, 1);
    

    workLayout->addWidget(sectionPages, 1);

    buildsLayout->addWidget(workspace, 1);
    pages->addWidget(buildsPage);

    // Страница мультиплеера
    auto *multiplayerPage = new QWidget;
    auto *multiplayerLayout = new QVBoxLayout(multiplayerPage);
    multiplayerLayout->setContentsMargins(4, 4, 4, 4);
    multiplayerLayout->setSpacing(14);

    auto *customPanel = makePanel();
    auto *customLayout = new QVBoxLayout(customPanel);
    customLayout->setContentsMargins(14, 12, 14, 14);
    auto *customHeader = new QHBoxLayout;
    customHeader->addWidget(heading("Доступные серверы"));
    customHeader->addStretch();
    auto *addServer = new QPushButton("＋ Добавить сервер");
    addServer->setObjectName("primary");
    auto *deleteServer = new QPushButton("Удалить сервер");
    deleteServer->setObjectName("danger");
    deleteServer->setEnabled(false);
    customHeader->addWidget(addServer);
    customHeader->addWidget(deleteServer);
    customLayout->addLayout(customHeader);

    auto *serverList = new QListWidget;
    serverList->setSelectionMode(QAbstractItemView::SingleSelection);
    // Список и строки используют фон акцентной панели без отдельной тёмной подложки.
    serverList->setStyleSheet(R"(
        QListWidget { background:transparent; border:0; outline:0; }
        QListWidget::item { background:transparent; border:0; padding:2px; }
        QListWidget::item:selected { background:rgba(115, 157, 205, 45); border-radius:5px; }
    )");
    auto *emptyServers = new QLabel("Доступные серверы не добавлены.");
    emptyServers->setAlignment(Qt::AlignCenter);
    emptyServers->setStyleSheet("background:transparent;");

    // Официальный сервер отображается в общем списке и требует авторизации.
    constexpr int officialServerRole = Qt::UserRole + 1;
    auto *officialItem = new QListWidgetItem;
    officialItem->setData(Qt::UserRole, QString("Официальный сервер RRS"));
    officialItem->setData(officialServerRole, true);
    officialItem->setSizeHint(QSize(0, 58));
    serverList->addItem(officialItem);

    auto *officialServerRow = new QWidget;
    officialServerRow->setAttribute(Qt::WA_StyledBackground, true);
    officialServerRow->setObjectName("serverEntryRow");
    officialServerRow->setStyleSheet(
        "QWidget#serverEntryRow { background:transparent; "
        "border:1px solid #52657d; border-radius:7px; } "
        "QWidget#serverEntryRow QLabel { background:transparent; border:0; }");
    auto *officialServerRowLayout = new QHBoxLayout(officialServerRow);
    officialServerRowLayout->setContentsMargins(10, 5, 10, 5);
    auto *officialInfoLayout = new QVBoxLayout;
    officialInfoLayout->setSpacing(2);
    auto *officialNameLabel = new QLabel("Официальный сервер RRS");
    officialNameLabel->setStyleSheet("font-weight:600;");
    auto *officialAddressLabel = new QLabel("Официальный сервер");
    officialAddressLabel->setObjectName("muted");
    officialInfoLayout->addWidget(officialNameLabel);
    officialInfoLayout->addWidget(officialAddressLabel);
    officialServerRowLayout->addLayout(officialInfoLayout, 1);
    auto *officialStatusLabel = new QLabel("Требуется авторизация");
    officialStatusLabel->setObjectName("muted");
    officialStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    officialServerRowLayout->addWidget(officialStatusLabel);
    serverList->setItemWidget(officialItem, officialServerRow);

    const QString savedServersPath = QDir(simulatorRoot).filePath("cfg/saved-servers.xml");
    auto appendSavedServerToList = [serverList](const SavedMultiplayerServer &server) {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, server.name);
        item->setData(Qt::UserRole + 1, false);
        item->setData(Qt::UserRole + 2, server.host);
        item->setData(Qt::UserRole + 3, server.port);
        item->setSizeHint(QSize(0, 58)); serverList->addItem(item);
        auto *row = new QWidget; row->setAttribute(Qt::WA_StyledBackground, true);
        row->setObjectName("serverEntryRow");
        row->setStyleSheet("QWidget#serverEntryRow { background:transparent; border:1px solid #52657d; border-radius:7px; } QWidget#serverEntryRow QLabel { background:transparent; border:0; }");
        auto *layout = new QHBoxLayout(row); layout->setContentsMargins(10, 5, 10, 5);
        auto *info = new QVBoxLayout; info->setSpacing(2);
        auto *nameLabel = new QLabel(server.name); nameLabel->setStyleSheet("font-weight:600;");
        auto *addressLabel = new QLabel(QString("%1:%2").arg(server.host, server.port)); addressLabel->setObjectName("muted");
        info->addWidget(nameLabel); info->addWidget(addressLabel); layout->addLayout(info, 1);
        auto *status = new QLabel("Офлайн"); status->setObjectName("serverStatus"); status->setStyleSheet("color:#d9534f;"); status->setMinimumWidth(90); status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *ping = new QLabel("Ping: —"); ping->setObjectName("serverPing"); ping->setStyleSheet("color:#8b95a5;"); ping->setMinimumWidth(82); ping->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(status); layout->addWidget(ping); serverList->setItemWidget(item, row);
    };
    for (const auto &savedServer : loadSavedMultiplayerServers(savedServersPath)) appendSavedServerToList(savedServer);

    // Обновление доступности кнопок после завершения асинхронной проверки сервера.
    auto updateServersCallback = std::make_shared<std::function<void()>>();

    // Проверяем доступность игрового TCP-порта и измеряем время подключения.
    auto checkServerPings = [serverList, &window, updateServersCallback] {
        struct PingState {
            bool icmpFinished = false;
            bool tcpFinished = false;
            bool hasResult = false;
        };

        for (int i = 0; i < serverList->count(); ++i) {
            auto *item = serverList->item(i);
            if (item->data(Qt::UserRole + 1).toBool()) continue;
            QWidget *row = serverList->itemWidget(item);
            if (!row) continue;
            auto *status = row->findChild<QLabel *>("serverStatus");
            auto *ping = row->findChild<QLabel *>("serverPing");
            if (!status || !ping) continue;

            const QString host = item->data(Qt::UserRole + 2).toString();
            bool portOk = false;
            const quint16 port = item->data(Qt::UserRole + 3).toString().toUShort(&portOk);
            if (host.isEmpty()) {
                status->setText("Офлайн"); status->setStyleSheet("color:#d9534f;");
                ping->setText("Ping: —"); ping->setStyleSheet("color:#8b95a5;");
                continue;
            }

            status->setText("Офлайн"); status->setStyleSheet("color:#d9534f;");
            item->setData(Qt::UserRole + 4, false);
            if (*updateServersCallback) (*updateServersCallback)();
            // Не показываем промежуточное многоточие во время проверки.
            // До получения нового результата сохраняем последнее значение Ping;
            // при неудачной проверке finishIfNoResult установит прочерк.
            QPointer<QLabel> statusGuard(status), pingGuard(ping);
            QPointer<QWidget> rowGuard(row);
            // Номер поколения не позволяет запоздавшей проверке перезаписать
            // результат более новой проверки этого же сервера.
            const quint64 checkGeneration = row->property("pingCheckGeneration").toULongLong() + 1;
            row->setProperty("pingCheckGeneration", QVariant::fromValue<qulonglong>(checkGeneration));
            auto isCurrentCheck = [rowGuard, checkGeneration] {
                return rowGuard && rowGuard->property("pingCheckGeneration").toULongLong() == checkGeneration;
            };
            auto state = std::make_shared<PingState>();
            if (!portOk || port == 0) state->tcpFinished = true;

            auto finishIfNoResult = [state, pingGuard, isCurrentCheck] {
                if (isCurrentCheck() && state->icmpFinished && state->tcpFinished && !state->hasResult && pingGuard) {
                    pingGuard->setText("Ping: —");
                    pingGuard->setStyleSheet("color:#8b95a5;");
                }
            };

            // Ping показываем только при доступном игровом TCP-порту.
            // ICMP-проверка удалена: она могла показывать задержку узла,
            // даже когда игровой сервер остановлен.
            state->icmpFinished = true;
            if (!portOk || port == 0) {
                finishIfNoResult();
                continue;
            }

            auto *socket = new QTcpSocket(&window);
            auto tcpTimer = std::make_shared<QElapsedTimer>();
            tcpTimer->start();
            QObject::connect(socket, &QTcpSocket::connected, &window,
                [socket, statusGuard, pingGuard, tcpTimer, state, finishIfNoResult, isCurrentCheck, item, updateServersCallback] {
                    const qint64 elapsedMs = tcpTimer->elapsed();
                    if (isCurrentCheck() && statusGuard) {
                        item->setData(Qt::UserRole + 4, true);
                        statusGuard->setText("Онлайн");
                        statusGuard->setStyleSheet("color:#35b56a; font-weight:600;");
                        if (*updateServersCallback) (*updateServersCallback)();
                    }
                    // Успешное TCP-подключение само по себе является измерением задержки.
                    // Не подавляем его флагом ICMP: даже если ICMP-ответ был получен,
                    // TCP-результат должен гарантированно обновить видимый Ping при Online.
                    state->hasResult = true;
                    if (isCurrentCheck() && pingGuard) {
                        pingGuard->setText(QString("Ping: %1 мс").arg(elapsedMs));
                        pingGuard->setStyleSheet("color:#35b56a; font-weight:600;");
                    }
                    state->tcpFinished = true;
                    finishIfNoResult();
                    socket->disconnectFromHost();
                    socket->deleteLater();
                });
            QObject::connect(socket, &QTcpSocket::errorOccurred, &window,
                [socket, state, finishIfNoResult, item, updateServersCallback](QAbstractSocket::SocketError) {
                    item->setData(Qt::UserRole + 4, false);
                    if (*updateServersCallback) (*updateServersCallback)();
                    state->tcpFinished = true;
                    finishIfNoResult();
                    socket->deleteLater();
                });
            socket->connectToHost(host, port);
            QTimer::singleShot(3000, socket, [socket, state, finishIfNoResult, item, updateServersCallback] {
                if (socket->state() == QAbstractSocket::ConnectingState) {
                    item->setData(Qt::UserRole + 4, false);
                    if (*updateServersCallback) (*updateServersCallback)();
                    socket->abort();
                    state->tcpFinished = true;
                    finishIfNoResult();
                    socket->deleteLater();
                }
            });
        }
    };

    customLayout->addWidget(serverList, 1);
    customLayout->addWidget(emptyServers, 1);

    multiplayerLayout->addWidget(customPanel, 1);

    // Действия с выбранным пользовательским сервером вынесены
    // за пределы панели «Пользовательские серверы».
    auto *serverActions = new QHBoxLayout;
    serverActions->setSpacing(8);
    auto *viewerButton = new QPushButton("▶ Запустить Viewer");
    auto *routeMapButton = new QPushButton("⌖ Запустить карту маршрута");
    viewerButton->setEnabled(false);
    routeMapButton->setEnabled(false);
    serverActions->addWidget(viewerButton, 1);
    serverActions->addWidget(routeMapButton, 1);
    multiplayerLayout->addLayout(serverActions);

    auto *multiplayerViewerProcess = new QProcess(&window);
    auto *multiplayerRouteMapProcess = new QProcess(&window);

    // Реальная авторизация будет подключена вместе с сервисом учётных записей.
    // Пока официальный сервер остаётся недоступен, а пользовательские работают как прежде.
    auto updateServers = [serverList, emptyServers, deleteServer,
                          viewerButton, routeMapButton,
                          multiplayerViewerProcess, multiplayerRouteMapProcess] {
        const bool hasItems = serverList->count() > 0;
        const int selectedRow = serverList->currentRow();
        const bool hasSelection = selectedRow >= 0;
        const bool isOfficial = hasSelection &&
            serverList->item(selectedRow)->data(Qt::UserRole + 1).toBool();
        const bool isAuthenticated = false;
        const bool selectedServerOnline = hasSelection && !isOfficial &&
            serverList->item(selectedRow)->data(Qt::UserRole + 4).toBool();
        const bool canConnect = hasSelection && (isOfficial ? isAuthenticated : selectedServerOnline);
        serverList->setVisible(hasItems);
        emptyServers->setVisible(!hasItems);
        deleteServer->setEnabled(hasSelection && !isOfficial);
        viewerButton->setEnabled(canConnect &&
            multiplayerViewerProcess->state() == QProcess::NotRunning);
        routeMapButton->setEnabled(canConnect &&
            multiplayerRouteMapProcess->state() == QProcess::NotRunning);
    };
    *updateServersCallback = updateServers;

    QObject::connect(serverList, &QListWidget::currentRowChanged, &window,
                     [updateServers](int) { updateServers(); });

    const QString multiplayerBinDirectory = QDir(simulatorRoot).filePath("bin");
    QObject::connect(multiplayerViewerProcess, &QProcess::stateChanged, &window,
        [updateServers](QProcess::ProcessState) { updateServers(); });
    QObject::connect(multiplayerRouteMapProcess, &QProcess::stateChanged, &window,
        [updateServers](QProcess::ProcessState) { updateServers(); });
    QObject::connect(multiplayerViewerProcess,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &window,
        [updateServers](int, QProcess::ExitStatus) { updateServers(); });
    QObject::connect(multiplayerRouteMapProcess,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &window,
        [updateServers](int, QProcess::ExitStatus) { updateServers(); });

    auto launchMultiplayerCompanion = [&window, serverList, multiplayerBinDirectory](
            QProcess *process, const QString &executableName, const QString &displayName) {
        const int row = serverList->currentRow();
        if (row < 0) return;
        auto *item = serverList->currentItem();
        if (item->data(Qt::UserRole + 1).toBool() || !item->data(Qt::UserRole + 4).toBool()) return;
        if (process->state() != QProcess::NotRunning) {
            QMessageBox::information(&window, displayName, displayName + " уже запущен.");
            return;
        }
        const QString executable = QDir(multiplayerBinDirectory).filePath(executableName);
        if (!QFileInfo::exists(executable)) {
            QMessageBox::warning(&window, "Запуск приложения",
                QString("Не найден файл %1:\n%2").arg(executableName, executable));
            return;
        }
        const QString host = item->data(Qt::UserRole + 2).toString();
        const QString port = item->data(Qt::UserRole + 3).toString();
        process->setWorkingDirectory(multiplayerBinDirectory);
        process->setProgram(executable);
        process->setArguments({"--host-address", host, "--port", port});
        process->start();
        if (!process->waitForStarted(3000)) {
            QMessageBox::warning(&window, "Запуск приложения",
                QString("Не удалось запустить %1:\n%2").arg(displayName, process->errorString()));
        }
    };
    QObject::connect(viewerButton, &QPushButton::clicked, &window,
        [launchMultiplayerCompanion, multiplayerViewerProcess] {
            launchMultiplayerCompanion(multiplayerViewerProcess, "viewer.exe", "Viewer");
        });
    QObject::connect(routeMapButton, &QPushButton::clicked, &window,
        [launchMultiplayerCompanion, multiplayerRouteMapProcess] {
            launchMultiplayerCompanion(multiplayerRouteMapProcess, "route-map.exe", "Карта маршрута");
        });

    QObject::connect(addServer, &QPushButton::clicked, &window, [&window, serverList, updateServers, appendSavedServerToList, savedServersPath, checkServerPings] {
        QDialog dialog(&window);
        dialog.setWindowTitle("Добавить пользовательский сервер");
        dialog.setMinimumWidth(390);
        auto *form = new QFormLayout(&dialog);
        auto *name = new QLineEdit;
        name->setPlaceholderText("Например, Сервер друзей");
        auto *address = new QLineEdit;
        address->setPlaceholderText("IP-адрес или доменное имя");
        auto *port = new QSpinBox;
        port->setRange(1, 65535);
        port->setValue(27015);
        form->addRow("Название:", name);
        form->addRow("Адрес / IP:", address);
        form->addRow("Порт:", port);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, name, address] {
            if (name->text().trimmed().isEmpty() || address->text().trimmed().isEmpty()) {
                QMessageBox::warning(&dialog, "Не заполнены данные", "Укажите название и адрес сервера.");
                return;
            }
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;
        SavedMultiplayerServer newServer{name->text().trimmed(), address->text().trimmed(), QString::number(port->value())};
        auto servers = loadSavedMultiplayerServers(savedServersPath);
        servers.append(newServer);
        if (!saveMultiplayerServers(savedServersPath, servers)) {
            QMessageBox::critical(&window, "Ошибка сохранения", QString("Не удалось сохранить список серверов в %1").arg(savedServersPath));
            return;
        }
        appendSavedServerToList(newServer);
        checkServerPings();
        serverList->setCurrentRow(serverList->count() - 1);
        updateServers();
    });

    QObject::connect(deleteServer, &QPushButton::clicked, &window, [&window, serverList, updateServers, savedServersPath] {
        const int row = serverList->currentRow();
        if (row < 0 || serverList->item(row)->data(Qt::UserRole + 1).toBool()) return;
        const QString serverName = serverList->item(row)->data(Qt::UserRole).toString();
        const auto answer = QMessageBox::warning(&window, "Удаление сервера",
            QString("Удалить сервер «%1» из списка?").arg(serverName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        QList<SavedMultiplayerServer> remaining;
        for (int i = 0; i < serverList->count(); ++i) {
            auto *entry = serverList->item(i);
            if (i == row || entry->data(Qt::UserRole + 1).toBool()) continue;
            remaining.append({entry->data(Qt::UserRole).toString(), entry->data(Qt::UserRole + 2).toString(), entry->data(Qt::UserRole + 3).toString()});
        }
        if (!saveMultiplayerServers(savedServersPath, remaining)) {
            QMessageBox::critical(&window, "Ошибка сохранения", QString("Не удалось обновить список серверов в %1").arg(savedServersPath));
            return;
        }
        delete serverList->takeItem(row);
        if (serverList->count() > 0)
            serverList->setCurrentRow(qMin(row, serverList->count() - 1));
        updateServers();
    });
    updateServers();
    auto *serverPingTimer = new QTimer(&window);
    serverPingTimer->setInterval(5000);
    QObject::connect(serverPingTimer, &QTimer::timeout, &window, [pages, multiplayerPage, checkServerPings] {
        if (pages->currentWidget() == multiplayerPage) checkServerPings();
    });
    QObject::connect(pages, &QStackedWidget::currentChanged, &window,
        [pages, multiplayerPage, serverPingTimer, checkServerPings](int) {
            if (pages->currentWidget() == multiplayerPage) {
                checkServerPings();
                if (!serverPingTimer->isActive()) serverPingTimer->start();
            } else serverPingTimer->stop();
        });
    pages->addWidget(multiplayerPage);
    checkServerPings();

    auto makeSectionPage = [](const QString &title, const QString &message) {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(4, 4, 4, 4);
        auto *panel = makePanel();
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(16, 14, 16, 16);
        panelLayout->addWidget(heading(title));
        auto *label = new QLabel(message);
        label->setAlignment(Qt::AlignCenter);
        label->setObjectName("muted");
        label->setWordWrap(true);
        panelLayout->addWidget(label, 1);
        layout->addWidget(panel, 1);
        return page;
    };

    // Галерея скриншотов: миниатюры загружаются из папки screenshots рядом с лаунчером.
    auto *screenshotsPage = new QWidget;
    auto *screenshotsLayout = new QVBoxLayout(screenshotsPage);
    screenshotsLayout->setContentsMargins(4, 4, 4, 4);
    auto *screenshotsPanel = makePanel();
    auto *screenshotsPanelLayout = new QVBoxLayout(screenshotsPanel);
    screenshotsPanelLayout->setContentsMargins(16, 14, 16, 16);
    auto *screenshotsHeader = new QHBoxLayout;
    screenshotsHeader->addWidget(heading("Скриншоты"));
    screenshotsHeader->addStretch(1);
    auto *refreshScreenshotsButton = new QPushButton("Обновить");
    screenshotsHeader->addWidget(refreshScreenshotsButton);
    screenshotsPanelLayout->addLayout(screenshotsHeader);

    auto *screenshotsList = new QListWidget;
    screenshotsList->setViewMode(QListView::IconMode);
    screenshotsList->setFlow(QListView::LeftToRight);
    screenshotsList->setWrapping(true);
    screenshotsList->setResizeMode(QListView::Adjust);
    screenshotsList->setMovement(QListView::Static);
    screenshotsList->setIconSize(QSize(180, 120));
    screenshotsList->setGridSize(QSize(210, 165));
    screenshotsList->setSpacing(8);
    screenshotsList->setUniformItemSizes(true);
    screenshotsList->setSelectionMode(QAbstractItemView::SingleSelection);
    screenshotsList->setStyleSheet(
        "QListWidget { background:#202c39; border:1px solid #40546a; border-radius:7px; padding:8px; }"
        "QListWidget::item { background:#2b3949; border:1px solid #52657d; border-radius:7px; padding:6px; color:#edf3fa; }"
        "QListWidget::item:selected { background:#315879; border:1px solid #65a9dc; }"
        "QListWidget::item:hover { border:1px solid #79b9e8; }");
    screenshotsPanelLayout->addWidget(screenshotsList, 1);
    screenshotsLayout->addWidget(screenshotsPanel, 1);

    auto screenshotsFiles = [simulatorRoot] {
        // Основное расположение — screenshots в выбранной корневой папке RRS.
        // Дополнительные варианты оставлены для разных способов установки Launcher.
        const QDir appDir(QCoreApplication::applicationDirPath());
        const QDir currentDir(QDir::currentPath());
        const QStringList candidates = {
            QDir(simulatorRoot).filePath("screenshots"),
            QDir(simulatorRoot).filePath("Screenshots"),
            QDir(simulatorRoot).filePath("bin/screenshots"),
            appDir.filePath("screenshots"),
            appDir.filePath("../screenshots"),
            appDir.filePath("../../screenshots"),
            currentDir.filePath("screenshots"),
            currentDir.filePath("../screenshots"),
            currentDir.filePath("../../screenshots")
        };
        QStringList supportedFiles;
        QSet<QString> seen;
        for (const QString &candidate : candidates) {
            const QDir dir(candidate);
            if (!dir.exists()) continue;
            const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
            for (const QFileInfo &entry : entries) {
                const QString suffix = entry.suffix().toLower();
                if (suffix != "png" && suffix != "jpg" && suffix != "jpeg" &&
                    suffix != "bmp" && suffix != "webp" && suffix != "gif") continue;
                const QString absolutePath = entry.absoluteFilePath();
                if (seen.contains(absolutePath)) continue;
                seen.insert(absolutePath);
                supportedFiles.append(absolutePath);
            }
            // Приоритет у первого найденного каталога с изображениями.
            if (!supportedFiles.isEmpty()) break;
        }
        return supportedFiles;
    };
    auto refreshScreenshots = [screenshotsList, screenshotsFiles] {
        screenshotsList->clear();
        QStringList files = screenshotsFiles();
        std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
            return QString::compare(QFileInfo(a).fileName(), QFileInfo(b).fileName(), Qt::CaseInsensitive) < 0;
        });
        for (const QString &imagePath : files) {
            const QFileInfo fileInfo(imagePath);
            QPixmap pixmap(imagePath);
            if (pixmap.isNull()) continue;
            auto *item = new QListWidgetItem(QIcon(pixmap.scaled(180, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation)),
                                             fileInfo.fileName());
            item->setData(Qt::UserRole, imagePath);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            item->setToolTip(fileInfo.fileName());
            screenshotsList->addItem(item);
        }
        if (screenshotsList->count() == 0) {
            auto *emptyItem = new QListWidgetItem("В папке screenshots пока нет изображений");
            emptyItem->setFlags(Qt::NoItemFlags);
            screenshotsList->addItem(emptyItem);
        }
    };
    QObject::connect(refreshScreenshotsButton, &QPushButton::clicked, &window, refreshScreenshots);
    QObject::connect(screenshotsList, &QListWidget::itemDoubleClicked, &window,
                     [&window, screenshotsList, screenshotsFiles](QListWidgetItem *item) {
        QStringList imagePaths = screenshotsFiles();
        const QString initialPath = item->data(Qt::UserRole).toString();
        if (initialPath.isEmpty() || imagePaths.isEmpty()) return;
        int currentIndex = imagePaths.indexOf(initialPath);
        if (currentIndex < 0) return;

        QDialog preview(&window);
        preview.setWindowTitle(QFileInfo(initialPath).fileName());
        preview.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                               | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                               | Qt::WindowCloseButtonHint);
        preview.setSizeGripEnabled(true);
        preview.setMinimumSize(520, 360);
        preview.resize(1000, 700);
        auto *previewLayout = new QVBoxLayout(&preview);
        auto *galleryLayout = new QHBoxLayout;
        auto *previousButton = new QToolButton(&preview);
        previousButton->setText("‹");
        previousButton->setToolTip("Предыдущий скриншот");
        previousButton->setFixedWidth(42);
        auto *imageLabel = new ScaledScreenshotLabel(QPixmap(imagePaths.at(currentIndex)));
        auto *nextButton = new QToolButton(&preview);
        nextButton->setText("›");
        nextButton->setToolTip("Следующий скриншот");
        nextButton->setFixedWidth(42);
        galleryLayout->addWidget(previousButton);
        galleryLayout->addWidget(imageLabel, 1);
        galleryLayout->addWidget(nextButton);
        previewLayout->addLayout(galleryLayout, 1);
        auto *counterLabel = new QLabel(&preview);
        counterLabel->setAlignment(Qt::AlignCenter);
        previewLayout->addWidget(counterLabel);
        auto updateImage = [&]() {
            const QString path = imagePaths.at(currentIndex);
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                imageLabel->setSourcePixmap(pixmap);
                preview.setWindowTitle(QFileInfo(path).fileName());
            }
            counterLabel->setText(QString("%1 / %2").arg(currentIndex + 1).arg(imagePaths.size()));
        };
        auto moveImage = [&](int delta) {
            currentIndex = (currentIndex + delta + imagePaths.size()) % imagePaths.size();
            updateImage();
        };
        imageLabel->setWheelHandler(moveImage);
        QObject::connect(previousButton, &QToolButton::clicked, &preview, [&]() { moveImage(-1); });
        QObject::connect(nextButton, &QToolButton::clicked, &preview, [&]() { moveImage(1); });
        auto *bottomLayout = new QHBoxLayout;
        bottomLayout->addStretch();
        auto *closeButton = new QPushButton("Закрыть");
        QObject::connect(closeButton, &QPushButton::clicked, &preview, &QDialog::accept);
        bottomLayout->addWidget(closeButton);
        previewLayout->addLayout(bottomLayout);
        updateImage();
        preview.exec();
    });
    refreshScreenshots();

    // Страница настроек: навигация слева, содержимое выбранного раздела справа.
    auto *settingsPage = new QWidget;
    auto *settingsLayout = new QHBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(4, 4, 4, 4);
    settingsLayout->setSpacing(12);

    auto *settingsNavPanel = makePanel();
    settingsNavPanel->setFixedWidth(205);
    auto *settingsNavLayout = new QVBoxLayout(settingsNavPanel);
    settingsNavLayout->setContentsMargins(8, 8, 8, 8);
    settingsNavLayout->setSpacing(8);
    settingsNavLayout->setAlignment(Qt::AlignTop);

    auto *settingsContentPanel = makePanel();
    // В настройках фон между группами должен быть ровным, без общего градиента карточки.
    settingsContentPanel->setStyleSheet(
        "QFrame#panel, QFrame#panel:hover, QFrame#panel:focus, QFrame#panel:pressed { "
        "background:#202c39; border:1px solid #40546a; border-radius:9px; }");
    auto *settingsContentLayout = new QVBoxLayout(settingsContentPanel);
    settingsContentLayout->setContentsMargins(16, 14, 16, 16);

    auto *settingsStack = new QStackedWidget;
    settingsStack->setStyleSheet("QStackedWidget { background:#202c39; border:0; }");
    std::function<void()> resetCurrentSettings;
    std::function<void()> applyCurrentSettings;
    std::function<bool()> graphicsHasUnsavedChanges = [] { return false; };
    std::function<bool()> saveGraphicsSettings = [] { return true; };
    std::function<void()> reloadGraphicsSettings = [] {};
    QList<std::function<void()>> resetActions;
    QList<std::function<void()>> applyActions;
    QComboBox *interfaceLanguageCombo = nullptr;
    const QString launcherConfigPath = QDir(simulatorRoot).filePath("cfg/RM_launcher.xml");
    const QStringList settingsNames = {"Основное", "Язык", "Внешний вид", "Графика", "Сеть"};
    const QList<QStyle::StandardPixmap> settingsIcons = {
        QStyle::SP_ComputerIcon,
        QStyle::SP_FileDialogInfoView,
        QStyle::SP_FileDialogContentsView,
        QStyle::SP_DesktopIcon,
        QStyle::SP_DriveNetIcon
    };
    const QStringList settingsDescriptions = {
        "Основные параметры лаунчера.",
        "Выбор языка интерфейса.",
        "Настройки оформления и темы лаунчера.",
        "Параметры графики и отображения.",
        "Параметры сетевого подключения."
    };
    auto *settingsButtonGroup = new QButtonGroup(settingsPage);
    settingsButtonGroup->setExclusive(true);
    for (int i = 0; i < settingsNames.size(); ++i) {
        auto *nav = new QPushButton(settingsNames[i]);
        nav->setIcon(makeSettingsIcon(i));
        nav->setIconSize(QSize(18, 18));
        nav->setLayoutDirection(Qt::LeftToRight);
        nav->setObjectName("navButton");
        nav->setStyleSheet("text-align: left;");
        nav->setCheckable(true);
        nav->setCursor(Qt::PointingHandCursor);
        nav->setMinimumHeight(42);
        nav->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        settingsButtonGroup->addButton(nav, i);
        settingsNavLayout->addWidget(nav, 0, Qt::AlignTop);

        QWidget *section = nullptr;
        if (i == 0) {
            // Основные параметры запуска оформлены в том же стиле, что и панели графики/сети.
            auto *mainSettingsPage = new QWidget;
            mainSettingsPage->setObjectName("mainSettingsPage");
            mainSettingsPage->setStyleSheet(R"(
                QWidget#mainSettingsPage { background:#202c39; border:0; }
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
                QCheckBox { background:transparent; color:#e7edf5; spacing:8px; }
                QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
                QCheckBox::indicator:checked { background:#202c39; border:1px solid #75baf0; }
                QCheckBox::indicator:unchecked:hover { border:1px solid #9bcdf4; background:#30465c; }
            )");

            auto *mainSettingsLayout = new QVBoxLayout(mainSettingsPage);
            mainSettingsLayout->setContentsMargins(2, 2, 8, 2);
            mainSettingsLayout->setSpacing(18);

            auto *launchParameters = new QGroupBox("Параметры запуска");
            launchParameters->setObjectName("launchParametersPanel");
            launchParameters->setAttribute(Qt::WA_StyledBackground, true);
            launchParameters->setAutoFillBackground(true);
            auto *launchParametersLayout = new QVBoxLayout(launchParameters);
            launchParametersLayout->setContentsMargins(8, 10, 8, 8);
            launchParametersLayout->setSpacing(12);

            auto *autoStartViewer = new GraphicsCheckBox("Автоматический запуск вьювера");
            auto *autoStartRouteMap = new GraphicsCheckBox("Автоматический запуск карты маршрута");
            autoStartViewer->setObjectName("autoStartViewerSetting");
            autoStartRouteMap->setObjectName("autoStartRouteMapSetting");
            const QString launcherXmlPath = QDir(simulatorRoot).filePath("cfg/RM_launcher.xml");
            QFile launcherConfigFile(launcherXmlPath);
            QString launcherConfigXml;
            if (launcherConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                launcherConfigXml = QString::fromUtf8(launcherConfigFile.readAll());
                launcherConfigFile.close();
            } else {
                QMessageBox::warning(mainSettingsPage, "Настройки Launcher",
                    QString("Не удалось прочитать конфигурацию Launcher:\n%1").arg(launcherXmlPath));
            }
            const QString autoViewerValue = readXmlSectionValue(launcherConfigXml, "Launcher_settings", "AutoStartViewer", "1");
            const QString autoRouteMapValue = readXmlSectionValue(launcherConfigXml, "Launcher_settings", "AutoStartRouteMap", "1");
            autoStartViewer->setChecked(autoViewerValue == "1");
            autoStartRouteMap->setChecked(autoRouteMapValue == "1");
            launchParametersLayout->addWidget(autoStartViewer);
            launchParametersLayout->addWidget(autoStartRouteMap);
            mainSettingsLayout->addWidget(launchParameters);
            mainSettingsLayout->addStretch(1);

            resetCurrentSettings = [autoStartViewer, autoStartRouteMap] {
                autoStartViewer->setChecked(true);
                autoStartRouteMap->setChecked(true);
            };
            applyCurrentSettings = [autoStartViewer, autoStartRouteMap, launcherXmlPath, mainSettingsPage] {
                QHash<QString, QString> values;
                values.insert("AutoStartViewer", autoStartViewer->isChecked() ? "1" : "0");
                values.insert("AutoStartRouteMap", autoStartRouteMap->isChecked() ? "1" : "0");
                QString error;
                if (!writeLauncherSettings(launcherXmlPath, values, &error))
                    QMessageBox::warning(mainSettingsPage, "Настройки Launcher", error);
            };
            section = mainSettingsPage;
        } else if (i == 1) {
            auto *languagePage = new QWidget;
            languagePage->setObjectName("languagePage");
            languagePage->setStyleSheet(R"(
                QWidget#languagePage { background:#202c39; border:0; }
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
                QLabel { background:transparent; color:#e7edf5; border:0; }
                QComboBox { background:#202c39; color:#e7edf5; border:1px solid #4b6279; border-radius:5px; padding:5px 8px; }
                QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }\n                QDateEdit { padding:5px 34px 5px 8px; }\n                QDateEdit::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; }\n                QDateEdit::down-arrow { image:none; width:0px; height:0px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }
            )");

            auto *languageLayout = new QVBoxLayout(languagePage);
            languageLayout->setContentsMargins(2, 2, 8, 2);
            languageLayout->setSpacing(18);

            auto *languageGroup = new QGroupBox("Язык лаунчера");
            languageGroup->setObjectName("languagePanel");
            languageGroup->setAttribute(Qt::WA_StyledBackground, true);
            languageGroup->setAutoFillBackground(true);
            auto *languageGroupLayout = new QVBoxLayout(languageGroup);
            languageGroupLayout->setContentsMargins(8, 10, 8, 8);
            languageGroupLayout->setSpacing(10);
            auto *languageRow = new QHBoxLayout;
            languageRow->setSpacing(14);
            auto *languageLabel = new QLabel("Язык интерфейса");
            languageLabel->setAutoFillBackground(false);
            languageLabel->setAttribute(Qt::WA_StyledBackground, false);
            languageLabel->setStyleSheet("background:transparent; color:#e7edf5; border:0; padding:0;");
            auto *languageCombo = new MainSettingsComboBox;
            interfaceLanguageCombo = languageCombo;
            languageCombo->addItems({"Русский", "English"});
            {
                QFile languageConfigFile(launcherConfigPath);
                QString languageXml;
                if (languageConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    languageXml = QString::fromUtf8(languageConfigFile.readAll());
                    languageConfigFile.close();
                }
                const QString languageCode = readXmlSectionValue(languageXml, "Launcher_settings", "InterfaceLanguage", "ru");
                languageCombo->setCurrentIndex(languageCode.compare("en", Qt::CaseInsensitive) == 0 ? 1 : 0);
            }
            languageRow->addWidget(languageLabel);
            languageRow->addStretch(1);
            languageRow->addWidget(languageCombo);
            languageGroupLayout->addLayout(languageRow);
            languageLayout->addWidget(languageGroup);
            languageLayout->addStretch(1);
            resetCurrentSettings = [languageCombo] { languageCombo->setCurrentIndex(0); };
            applyCurrentSettings = [languageCombo, launcherConfigPath, languagePage] {
                QHash<QString, QString> values;
                values.insert("InterfaceLanguage", languageCombo->currentIndex() == 1 ? "en" : "ru");
                QString error;
                if (!writeLauncherSettings(launcherConfigPath, values, &error))
                    QMessageBox::warning(languagePage, "Launcher settings", error);
            };
            section = languagePage;
        } else if (i == 2) {
            // Раздел оформления лаунчера: панель-заготовка в общем стиле настроек.
            auto *appearancePage = new QWidget;
            appearancePage->setObjectName("appearancePage");
            appearancePage->setStyleSheet(R"(
                QWidget#appearancePage { background:#202c39; border:0; }
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
                QLabel { background:transparent; color:#e7edf5; border:0; }
                QComboBox { background:#202c39; color:#e7edf5; border:1px solid #4b6279; border-radius:5px; padding:5px 8px; }
                QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }
            )");

            auto *appearanceLayout = new QVBoxLayout(appearancePage);
            appearanceLayout->setContentsMargins(2, 2, 8, 2);
            appearanceLayout->setSpacing(18);

            auto *appearanceGroup = new QGroupBox("Оформление лаунчера");
            appearanceGroup->setObjectName("appearanceLauncherPanel");
            appearanceGroup->setAttribute(Qt::WA_StyledBackground, true);
            appearanceGroup->setAutoFillBackground(true);
            auto *appearanceGroupLayout = new QVBoxLayout(appearanceGroup);
            appearanceGroupLayout->setContentsMargins(8, 10, 8, 8);
            appearanceGroupLayout->setSpacing(10);

            auto addAppearanceComboRow = [appearanceGroupLayout](const QString &labelText, QComboBox *combo) {
                auto *row = new QHBoxLayout;
                row->setSpacing(14);
                auto *label = new QLabel(labelText);
                // Не допускаем заливку QLabel цветом Base/Window при смене темы.
                // Подписи должны оставаться прозрачными на фоне родительской плашки.
                label->setAutoFillBackground(false);
                label->setAttribute(Qt::WA_StyledBackground, false);
                label->setStyleSheet("background:transparent; color:#e7edf5; border:0; padding:0;");
                row->addWidget(label);
                row->addStretch(1);
                row->addWidget(combo);
                appearanceGroupLayout->addLayout(row);
            };

            auto *themeCombo = new MainSettingsComboBox;
            QSettings launcherSettings("RRS", "Launcher");
            themeCombo->addItems({"Тёмная", "Светлая", "Системная"});
            themeCombo->setCurrentIndex(launcherSettings.value("appearance/theme", 0).toInt());
            themeCombo->setFixedWidth(500);
            themeCombo->setMinimumHeight(34);
            addAppearanceComboRow("Тема", themeCombo);

            auto *iconsCombo = new MainSettingsComboBox;
            iconsCombo->addItems({"Стандартные", "Минималистичные", "Классические"});
            iconsCombo->setCurrentIndex(0);
            iconsCombo->setFixedWidth(500);
            iconsCombo->setMinimumHeight(34);
            addAppearanceComboRow("Значки", iconsCombo);

            appearanceLayout->addWidget(appearanceGroup);
            appearanceLayout->addStretch(1);

            resetCurrentSettings = [themeCombo, iconsCombo] {
                themeCombo->setCurrentIndex(0);
                iconsCombo->setCurrentIndex(0);
            };
            applyCurrentSettings = [themeCombo, iconsCombo, &window, &app] {
                QSettings settings("RRS", "Launcher");
                settings.setValue("appearance/theme", themeCombo->currentIndex());
                settings.setValue("appearance/icons", iconsCombo->currentIndex());

                // Перекрашиваем все локальные стили виджетов, сохраняя синие акцентные цвета.
                const bool systemLight = app.styleHints()->colorScheme() == Qt::ColorScheme::Light;
                const bool light = themeCombo->currentIndex() == 1 ||
                    (themeCombo->currentIndex() == 2 && systemLight);
                const QList<QPair<QString, QString>> darkToLight = {
                    {"#1b222c","#f1f4f8"}, {"#18212b","#e8edf3"}, {"#1d2733","#f7f9fc"},
                    {"#1f2329","#f7f9fc"}, {"#1f252d","#edf2f7"}, {"#202329","#f1f4f8"},
                    {"#20262e","#e9eef4"}, {"#202a36","#e8edf4"}, {"#202b38","#f7f9fc"},
                    {"#202c39","#ffffff"}, {"#252a31","#edf1f6"}, {"#252b34","#f4f6f9"},
                    {"#252c35","#f7f9fc"}, {"#253241","#ffffff"}, {"#263241","#ffffff"},
                    {"#263748","#f1f5f9"}, {"#243140","#f2f6fa"}, {"#222e3b","#f0f4f8"},
                    {"#28313b","#e8edf3"}, {"#292e36","#f5f7fa"}, {"#293746","#e9eef4"},
                    {"#2b3949","#ffffff"}, {"#2b3b4e","#e5edf5"}, {"#2d4054","#e1eaf3"},
                    {"#303640","#f7f9fc"}, {"#303d4e","#edf2f7"}, {"#30465c","#e3ebf4"},
                    {"#343b46","#ffffff"}, {"#343d49","#edf2f7"}, {"#35404d","#e3eaf2"},
                    {"#35465a","#e6edf5"}, {"#3a414c","#cbd5df"}, {"#394757","#cbd5df"},
                    {"#34485c","#d3dce6"}, {"#3b4655","#e1e8f0"}, {"#40536a","#d7e0e9"},
                    {"#40546a","#cbd5df"}, {"#414b59","#e3eaf2"}, {"#465363","#c3cfdb"},
                    {"#465c73","#b9c8d8"}, {"#4b5563","#aebdcd"}, {"#4b5665","#9eafc1"},
                    {"#4b6279","#aebfd0"}, {"#526071","#536477"}, {"#52657d","#9fb2c6"},
                    {"#526b84","#9fb4ca"}, {"#536174","#9fb1c5"}, {"#596779","#9aacc0"},
                    {"#667487","#66788d"}, {"#718ba5","#71869d"},
                    {"#e7edf5","#202833"}, {"#e7eaf0","#202833"}, {"#e4edf7","#202833"},
                    {"#dce6f2","#263647"}, {"#dce8f5","#263647"}, {"#cbd5e1","#34475a"},
                    {"#b8c9df","#40546a"}, {"#aebfd2","#52657a"}, {"#aab3c0","#526174"},
                    {"#9fb1c6","#5d6f83"}, {"#8fa4bc","#62768b"}
                };
                const QList<QPair<QString, QString>> lightToDark = {
                    {"#f1f4f8","#1b222c"}, {"#e8edf3","#18212b"}, {"#f7f9fc","#1d2733"},
                    {"#edf2f7","#1f252d"}, {"#f1f4f8","#202329"}, {"#e9eef4","#20262e"},
                    {"#e8edf4","#202a36"}, {"#ffffff","#202c39"}, {"#edf1f6","#252a31"},
                    {"#f4f6f9","#252b34"}, {"#ffffff","#2b3949"}, {"#f2f6fa","#243140"},
                    {"#f0f4f8","#222e3b"}, {"#e8edf3","#28313b"}, {"#f5f7fa","#292e36"},
                    {"#e9eef4","#293746"}, {"#e5edf5","#2b3b4e"}, {"#e1eaf3","#2d4054"},
                    {"#e3ebf4","#30465c"}, {"#ffffff","#343b46"}, {"#edf2f7","#343d49"},
                    {"#e3eaf2","#35404d"}, {"#e6edf5","#35465a"}, {"#cbd5df","#3a414c"},
                    {"#cbd5df","#465363"}, {"#d3dce6","#34485c"}, {"#e1e8f0","#3b4655"},
                    {"#d7e0e9","#40536a"}, {"#aebfd0","#4b6279"}, {"#9fb4ca","#526b84"},
                    {"#e7edf5","#e7edf5"}, {"#202833","#e7edf5"}, {"#263647","#dce6f2"},
                    {"#34475a","#cbd5e1"}, {"#40546a","#b8c9df"}, {"#526174","#aab3c0"},
                    {"#5d6f83","#9fb1c6"}, {"#62768b","#8fa4bc"}
                };
                auto recolor = [light, &darkToLight, &lightToDark](QString css) {
                    const auto &map = light ? darkToLight : lightToDark;
                    // Placeholders prevent one replacement from being transformed again
                    // by a later pair (important for shared palette values).
                    QStringList tokens;
                    for (int i = 0; i < map.size(); ++i) {
                        const QString token = QString("__RRS_THEME_COLOR_%1__").arg(i);
                        css.replace(map[i].first, token, Qt::CaseInsensitive);
                        tokens.append(token);
                    }
                    for (int i = 0; i < map.size(); ++i)
                        css.replace(tokens[i], map[i].second, Qt::CaseInsensitive);
                    return css;
                };
                // Всегда перекрашиваем исходные стили, а не результат предыдущего
                // переключения. Иначе при повторной смене темы фоновые цвета
                // QGroupBox/панелей теряются и остаются только рамки.
                const auto widgets = window.findChildren<QWidget*>();
                for (QWidget *widget : widgets) {
                    if (!widget->property("rrs_baseStyleSheetCaptured").toBool()) {
                        widget->setProperty("rrs_baseStyleSheet", widget->styleSheet());
                        widget->setProperty("rrs_baseStyleSheetCaptured", true);
                    }
                    const QString baseCss = widget->property("rrs_baseStyleSheet").toString();
                    if (!baseCss.isEmpty()) widget->setStyleSheet(recolor(baseCss));
                }
                static bool windowBaseCaptured = false;
                static QString windowBaseStyle;
                if (!windowBaseCaptured) {
                    windowBaseStyle = window.styleSheet();
                    windowBaseCaptured = true;
                }
                window.setStyleSheet(recolor(windowBaseStyle));
                // Глобальный стиль приложения также всегда строится из исходной версии.
                static bool appBaseCaptured = false;
                static QString appBaseStyle;
                if (!appBaseCaptured) {
                    appBaseStyle = app.styleSheet();
                    appBaseCaptured = true;
                }
                app.setStyleSheet(recolor(appBaseStyle));
                // Popup-списки QComboBox имеют отдельный viewport и собственную палитру.
                // Явно задаём палитру для выпадающих элементов в обеих темах,
                // чтобы текст и фон пунктов не сливались после переключения.
                QPalette comboPalette = app.palette();
                if (light) {
                    comboPalette.setColor(QPalette::Base, QColor("#ffffff"));
                    comboPalette.setColor(QPalette::Text, QColor("#202833"));
                    comboPalette.setColor(QPalette::ButtonText, QColor("#202833"));
                    comboPalette.setColor(QPalette::Highlight, QColor("#354b68"));
                    comboPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
                } else {
                    comboPalette.setColor(QPalette::Base, QColor("#202c39"));
                    comboPalette.setColor(QPalette::Text, QColor("#e7edf5"));
                    comboPalette.setColor(QPalette::ButtonText, QColor("#e7edf5"));
                    comboPalette.setColor(QPalette::Highlight, QColor("#354b68"));
                    comboPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
                }
                const QString controlBorder = light ? QStringLiteral("#b8c1cc") : QStringLiteral("#4b6279");
                const QString controlBackground = light ? QStringLiteral("#e7ebf0") : QStringLiteral("#202c39");
                const QString controlText = light ? QStringLiteral("#202833") : QStringLiteral("#e7edf5");
                // В «Графике» и «Сети» стрелка уже рисуется GraphicsComboBox.
                // Делаем область кнопки прозрачной, без серой подложки и разделителя,
                // как в эталонных списках раздела «Главное». MainSettingsComboBox
                // намеренно исключён ниже: его оформление не меняем.
                const QString comboStyle = QString(
                    "QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:5px; padding:5px 8px; }"
                    "QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; }"
                    "QComboBox::drop-down:hover { background:transparent; }"
                    "QComboBox::down-arrow { image:none; width:0px; height:0px; }"
                ).arg(controlBackground, controlText, controlBorder);
                const QString spinStyle = QString(
                    "QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:5px; padding:5px 34px 5px 8px; }"
                    "QSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:30px; height:50%; margin:0; border:0; background:transparent; border-top-right-radius:5px; }"
                    "QSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:30px; height:50%; margin:0; border:0; background:transparent; border-bottom-right-radius:5px; }"
                    "QSpinBox::up-button:hover, QSpinBox::down-button:hover, QSpinBox::up-button:pressed, QSpinBox::down-button:pressed { background:transparent; border:0; }"
                ).arg(controlBackground, controlText, controlBorder, controlBackground);
                for (QComboBox *combo : window.findChildren<QComboBox*>()) {
                    // Настройки раздела «Главное» уже имеют нужную стрелку и стиль.
                    // Не трогаем их вообще; обновляем только остальные списки.
                    combo->setPalette(comboPalette);
                    if (!dynamic_cast<MainSettingsComboBox *>(combo))
                        combo->setStyleSheet(comboStyle);
                    if (combo->view()) combo->view()->setPalette(comboPalette);
                    combo->update();
                }
                for (QSpinBox *spin : window.findChildren<QSpinBox*>()) {
                    spin->setStyleSheet(spinStyle);
                    spin->update();
                }
                QPalette palette = app.palette();
                if (light) {
                    palette.setColor(QPalette::Window, QColor("#f1f3f6"));
                    palette.setColor(QPalette::Base, QColor("#ffffff"));
                    palette.setColor(QPalette::AlternateBase, QColor("#e7ebf0"));
                    palette.setColor(QPalette::Text, QColor("#202833"));
                    palette.setColor(QPalette::WindowText, QColor("#202833"));
                    palette.setColor(QPalette::Button, QColor("#ffffff"));
                    palette.setColor(QPalette::ButtonText, QColor("#202833"));
                } else {
                    palette.setColor(QPalette::Window, QColor("#202329"));
                    palette.setColor(QPalette::Base, QColor("#202c39"));
                    palette.setColor(QPalette::AlternateBase, QColor("#2b3949"));
                    palette.setColor(QPalette::Text, QColor("#e7edf5"));
                    palette.setColor(QPalette::WindowText, QColor("#e7edf5"));
                    palette.setColor(QPalette::Button, QColor("#2b3949"));
                    palette.setColor(QPalette::ButtonText, QColor("#e7edf5"));
                }
                app.setPalette(palette);

                // Навигационные кнопки имеют собственный стиль, поэтому задаём
                // контрастные цвета явно после общей перекраски QSS.
                const QString navText = light ? "#34465a" : "#e7edf5";
                const QString navBorder = light ? "#c5d1de" : "transparent";
                const QString navHover = light ? "#e4edf6" : "#2b3b4e";
                const QString navSelected = light ? "#d7e7f5" : "#294c6b";
                const QString navSelectedText = light ? "#20364c" : "#ffffff";
                const QString navCss = QString(
                    "QPushButton#navButton { background:transparent; color:%1; border:1px solid %2; border-radius:7px; padding:9px 15px; text-align:left; font-weight:600; }"
                    "QPushButton#navButton:hover { background:%3; color:%1; border:1px solid %4; padding:9px 15px; font-weight:600; }"
                    "QPushButton#navButton:pressed { background:%3; color:%1; border:1px solid %4; padding:9px 15px; font-weight:600; }"
                    "QPushButton#navButton:checked { background:%5; color:%6; border:1px solid %7; padding:9px 15px; font-weight:600; }"
                ).arg(navText, navBorder, navHover,
                     light ? "#b2c5d8" : "#4b6680",
                     navSelected, navSelectedText,
                     light ? "#8eacc9" : "#5aa3dc");
                for (QPushButton *button : window.findChildren<QPushButton*>()) {
                    if (button->objectName() == "navButton")
                        button->setStyleSheet(navCss);
                }

            };
            section = appearancePage;
        } else if (i == 3) {
            auto *graphicsPage = new QWidget;
            graphicsPage->setObjectName("graphicsPage");
            graphicsPage->setStyleSheet(R"(
                QWidget#graphicsPage { background:#202c39; border:0; border-radius:0px; }
                QStackedWidget { background:#202c39; border:0; }
                QScrollArea { background:#202c39; border:0; }
                QScrollArea > QWidget > QWidget { background:#202c39; }
                QWidget#graphicsBody { background:#202c39; border:0; }
                QGroupBox { background-color:#2b3949; border:1px solid #4b6279; border-radius:8px; margin-top:20px; padding:13px; color:#e7edf5; }
                QGroupBox#graphicsGpuPanel, QGroupBox#graphicsWindowPanel,
                QGroupBox#graphicsAaPanel, QGroupBox#graphicsDisplayPanel,
                QGroupBox#graphicsShadowsPanel { background-color:#2b3949; border:1px solid #4b6279; border-radius:8px; }
                QGroupBox::title { subcontrol-origin:margin; left:13px; padding:0 6px; color:#e7edf5; background:transparent; }
                QLabel, QCheckBox { background:transparent; color:#e7edf5; border:0; }
                QFormLayout QLabel { background:transparent; }
                QComboBox, QSpinBox { background:#202c39; color:#e7edf5; border:1px solid #4b6279; border-radius:5px; padding:5px 8px; }
                QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }
                QSpinBox { padding-right:34px; }
                QSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:30px; height:50%; margin:0; border:0; background:transparent; border-top-right-radius:5px; }
                QSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:30px; height:50%; margin:0; border:0; background:transparent; border-bottom-right-radius:5px; }
                QSpinBox::up-button:hover, QSpinBox::down-button:hover, QSpinBox::up-button:pressed, QSpinBox::down-button:pressed { background:transparent; border:0; }
                QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
                QCheckBox::indicator:checked { background:#202c39; border:1px solid #75baf0; image:none; }
                QCheckBox::indicator:checked:hover { background:#30465c; border:1px solid #b5dcff; }
                QCheckBox::indicator:unchecked:hover { border:1px solid #9bcdf4; background:#30465c; }
                QScrollBar:vertical { background:transparent; width:10px; margin:3px 1px 3px 3px; border-radius:5px; }
                QScrollBar::handle:vertical { background:#526b84; min-height:32px; border-radius:5px; }
                QScrollBar::handle:vertical:hover { background:#7298bc; }
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
                QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
                QScrollBar:horizontal { background:transparent; height:10px; border-radius:5px; }
                QScrollBar::handle:horizontal { background:#526b84; min-width:32px; border-radius:5px; }
                QScrollBar::handle:horizontal:hover { background:#7298bc; }
                QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
            )");
            auto *graphicsLayout = new QVBoxLayout(graphicsPage);
            graphicsLayout->setContentsMargins(0, 0, 0, 0);
            graphicsLayout->setSpacing(10);

            auto *scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setAttribute(Qt::WA_StyledBackground);
            scroll->setAutoFillBackground(true);
            scroll->viewport()->setAutoFillBackground(true);
            scroll->viewport()->setStyleSheet("background:#202c39; border:0;");
            auto *graphicsBody = new QWidget;
            graphicsBody->setObjectName("graphicsBody");
            auto *bodyLayout = new QVBoxLayout(graphicsBody);
            bodyLayout->setContentsMargins(2, 2, 8, 2);
            bodyLayout->setSpacing(18);

            auto *gpuGroup = new QGroupBox("Текущий графический процессор");
            gpuGroup->setObjectName("graphicsGpuPanel");
            gpuGroup->setAttribute(Qt::WA_StyledBackground, true);
            gpuGroup->setAutoFillBackground(true);
            gpuGroup->setStyleSheet(R"(
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QLabel, QCheckBox {
                    background:transparent;
                    color:#e7edf5;
                    border:0;
                }
                QFormLayout QLabel {
                    background:transparent;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
            )");
            auto *gpuLayout = new QHBoxLayout(gpuGroup);
            auto *gpuLabel = new QLabel("Предпочитаемый графический процессор");
            gpuLabel->setStyleSheet("background:transparent; color:#e7edf5; border:0;");
            auto *gpuCombo = new GraphicsComboBox;
            gpuCombo->addItem("Автоматический выбор");
            const QStringList detectedAdapters = detectGraphicsAdapters();
            if (detectedAdapters.isEmpty()) {
                gpuCombo->addItem("Графические устройства не обнаружены");
                gpuCombo->setItemData(1, false, Qt::UserRole - 1);
            } else {
                gpuCombo->addItems(detectedAdapters);
            }
            gpuLayout->addWidget(gpuLabel);
            gpuLayout->addStretch(1);
            gpuLayout->addWidget(gpuCombo, 0, Qt::AlignRight);
            bodyLayout->addWidget(gpuGroup);

            auto *windowGroup = new QGroupBox("Настройки окна");
            windowGroup->setObjectName("graphicsWindowPanel");
            windowGroup->setAttribute(Qt::WA_StyledBackground, true);
            windowGroup->setAutoFillBackground(true);
            windowGroup->setStyleSheet(R"(
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QLabel, QCheckBox {
                    background:transparent;
                    color:#e7edf5;
                    border:0;
                }
                QFormLayout QLabel {
                    background:transparent;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
            )");
            auto *windowForm = new QFormLayout(windowGroup);
            auto *resolutionCombo = new GraphicsComboBox;
            resolutionCombo->addItems({
                "1280 × 720 (HD)",
                "1366 × 768",
                "1600 × 900",
                "1920 × 1080 (Full HD)",
                "2560 × 1440 (QHD)",
                "3840 × 2160 (4K)"
            });
            resolutionCombo->setCurrentText("1280 × 720 (HD)");
            windowForm->addRow("Разрешение", resolutionCombo);
            windowForm->setAlignment(resolutionCombo, Qt::AlignRight);
            auto *fullscreen = new GraphicsCheckBox("Полноэкранный режим");
            auto *windowTitle = new GraphicsCheckBox("Заголовок окна");
            windowTitle->setChecked(true);
            auto *monitor = new GraphicsSpinBox;
            monitor->setRange(0, 16);
            monitor->setValue(0);
            windowForm->addRow("Номер монитора", monitor);
            windowForm->setAlignment(monitor, Qt::AlignRight);
            windowForm->addRow(fullscreen);
            windowForm->addRow(windowTitle);
            auto *verticalSync = new GraphicsCheckBox("Вертикальная синхронизация (V-Sync)");
            verticalSync->setChecked(true);
            windowForm->addRow(verticalSync);

            // Ограничение частоты кадров: переключатель слева, числовое поле справа.
            auto *fpsRow = new QWidget(windowGroup);
            // Прозрачной остаётся только строка; само числовое поле сохраняет
            // стандартную подложку GraphicsSpinBox шириной 500 px.
            fpsRow->setObjectName("fpsLimitRow");
            fpsRow->setAttribute(Qt::WA_StyledBackground, true);
            fpsRow->setAutoFillBackground(false);
            fpsRow->setStyleSheet("QWidget#fpsLimitRow { background:transparent; border:0; }");
            auto *fpsLayout = new QHBoxLayout(fpsRow);
            fpsLayout->setContentsMargins(0, 0, 0, 0);
            fpsLayout->setSpacing(12);
            auto *fpsLimit = new GraphicsCheckBox("Ограничение частоты FPS");
            auto *fpsValue = new GraphicsSpinBox;
            fpsValue->setRange(1, 1000);
            fpsValue->setValue(60);
            fpsValue->setEnabled(false);
            QObject::connect(fpsLimit, &QCheckBox::toggled, fpsValue, &QWidget::setEnabled);
            fpsLayout->addWidget(fpsLimit);
            fpsLayout->addStretch(1);
            fpsLayout->addWidget(fpsValue, 0, Qt::AlignRight);
            windowForm->addRow(fpsRow);
            bodyLayout->addWidget(windowGroup);

            auto *aaGroup = new QGroupBox("Сглаживание");
            aaGroup->setObjectName("graphicsAaPanel");
            aaGroup->setAttribute(Qt::WA_StyledBackground, true);
            aaGroup->setAutoFillBackground(true);
            aaGroup->setStyleSheet(R"(
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QLabel, QCheckBox {
                    background:transparent;
                    color:#e7edf5;
                    border:0;
                }
                QFormLayout QLabel {
                    background:transparent;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
            )");
            auto *aaForm = new QFormLayout(aaGroup);
            auto *aaCombo = new GraphicsComboBox;
            aaCombo->addItems({"Выключено", "2x", "4x", "8x"});
            aaCombo->setCurrentText("4x");
            aaForm->addRow("MSAA", aaCombo);
            aaForm->setAlignment(aaCombo, Qt::AlignRight);
            bodyLayout->addWidget(aaGroup);

            auto *displayGroup = new QGroupBox("Настройки отображения");
            displayGroup->setObjectName("graphicsDisplayPanel");
            displayGroup->setAttribute(Qt::WA_StyledBackground, true);
            displayGroup->setAutoFillBackground(true);
            displayGroup->setStyleSheet(R"(
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QLabel, QCheckBox {
                    background:transparent;
                    color:#e7edf5;
                    border:0;
                }
                QFormLayout QLabel {
                    background:transparent;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
            )");
            auto *displayForm = new QFormLayout(displayGroup);
            auto *viewDistance = new GraphicsSpinBox;
            viewDistance->setRange(100, 50000);
            viewDistance->setSingleStep(500);
            viewDistance->setValue(2000);
            viewDistance->setSuffix(" м");
            displayForm->addRow("Дистанция видимости", viewDistance);
            displayForm->setAlignment(viewDistance, Qt::AlignRight);
            bodyLayout->addWidget(displayGroup);

            auto *shadowGroup = new QGroupBox("Тени");
            shadowGroup->setObjectName("graphicsShadowsPanel");
            shadowGroup->setAttribute(Qt::WA_StyledBackground, true);
            shadowGroup->setAutoFillBackground(true);
            shadowGroup->setStyleSheet(R"(
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QLabel, QCheckBox {
                    background:transparent;
                    color:#e7edf5;
                    border:0;
                }
                QFormLayout QLabel {
                    background:transparent;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
            )");
            auto *shadowLayout = new QVBoxLayout(shadowGroup);
            auto *shadowsEnabled = new GraphicsCheckBox("Включить тени");
            shadowLayout->addWidget(shadowsEnabled);
            auto addShadowComboRow = [shadowLayout](const QString &labelText, QComboBox *combo) {
                auto *row = new QHBoxLayout;
                row->setSpacing(14);
                auto *label = new QLabel(labelText);
                row->addWidget(label);
                row->addStretch(1);
                row->addWidget(combo);
                shadowLayout->addLayout(row);
            };
            auto *shadowQuality = new GraphicsComboBox;
            shadowQuality->addItems({"Низкое", "Среднее", "Высокое", "Ультра", "Пользовательское"});
            shadowQuality->setCurrentText("Пользовательское");
            addShadowComboRow("Качество теней", shadowQuality);
            auto *shadowResolution = new GraphicsComboBox;
            shadowResolution->addItems({"Низкое", "Среднее", "Высокое", "Ультра"});
            shadowResolution->setCurrentText("Среднее");
            addShadowComboRow("Разрешение теней", shadowResolution);
            auto *cascades = new GraphicsComboBox;
            cascades->addItems({"1", "2", "3", "4", "5", "6"});
            cascades->setCurrentText("3");
            addShadowComboRow("Число каскадов теней", cascades);
            auto *shadowDistance = new GraphicsComboBox;
            shadowDistance->addItems({"25", "30", "50", "75", "100", "125", "150", "175", "200"});
            shadowDistance->setCurrentText("30");
            addShadowComboRow("Дальность отрисовки теней", shadowDistance);
            auto setShadowControlsEnabled = [shadowQuality, shadowResolution, cascades, shadowDistance](bool enabled) {
                shadowQuality->setEnabled(enabled);
                shadowResolution->setEnabled(enabled);
                cascades->setEnabled(enabled);
                shadowDistance->setEnabled(enabled);
            };
            // Параметры теней доступны для редактирования независимо от флажка.
            // Флажок сохраняется как отдельный параметр включения теней.
            setShadowControlsEnabled(true);
            bodyLayout->addWidget(shadowGroup);
            bodyLayout->addStretch(1);
            scroll->setWidget(graphicsBody);
            graphicsLayout->addWidget(scroll, 1);

            const QString settingsXmlPath = QDir(simulatorRoot).filePath("cfg/settings.xml");
            auto readGraphicsValues = [settingsXmlPath]() {
                QFile file(settingsXmlPath);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
                return QString::fromUtf8(file.readAll());
            };
            auto applyXmlToControls = [=](const QString &xml) {
                if (xml.isEmpty()) return;
                const int width = readXmlSectionValue(xml, "Viewer", "Width", "1280").toInt();
                const int height = readXmlSectionValue(xml, "Viewer", "Height", "720").toInt();
                const QString resolutionPrefix = QString("%1 × %2").arg(width).arg(height);
                int resolutionIndex = -1;
                for (int i = 0; i < resolutionCombo->count(); ++i) {
                    if (resolutionCombo->itemText(i).startsWith(resolutionPrefix)) { resolutionIndex = i; break; }
                }
                if (resolutionIndex < 0) {
                    resolutionCombo->addItem(resolutionPrefix);
                    resolutionIndex = resolutionCombo->count() - 1;
                }
                resolutionCombo->setCurrentIndex(resolutionIndex);

                auto xmlBool = [&xml](const QString &tag, bool fallback) {
                    const QString value = readXmlSectionValue(xml, "Viewer", tag, fallback ? "1" : "0").toLower();
                    return value == "1" || value == "true" || value == "yes";
                };
                fullscreen->setChecked(xmlBool("FullScreen", false));
                monitor->setValue(readXmlSectionValue(xml, "Viewer", "ScreenNumber", "0").toInt());
                verticalSync->setChecked(xmlBool("VSync", true));
                const int fps = readXmlSectionValue(xml, "Viewer", "MaxFPS", "0").toInt();
                fpsLimit->setChecked(fps > 0);
                fpsValue->setValue(qBound(1, fps > 0 ? fps : 60, 1000));
                fpsValue->setEnabled(fps > 0);
                const int samples = readXmlSectionValue(xml, "Viewer", "Samples", "0").toInt();
                const QString aaText = samples <= 0 ? "Выключено" : QString("%1x").arg(samples);
                int aaIndex = aaCombo->findText(aaText);
                if (aaIndex < 0) { aaCombo->addItem(aaText); aaIndex = aaCombo->count() - 1; }
                aaCombo->setCurrentIndex(aaIndex);
                viewDistance->setValue(qBound(100, readXmlSectionValue(xml, "Viewer", "ViewDistance", "2000").toInt(), 50000));
                shadowsEnabled->setChecked(xmlBool("Shadow", false));

                const int preset = readXmlSectionValue(xml, "Viewer", "ShadowsPreset", "4").toInt();
                shadowQuality->setCurrentIndex(qBound(0, preset, 4));
                const int shadowSamples = readXmlSectionValue(xml, "Viewer", "ShadowResolution", "1024").toInt();
                const QMap<int, QString> resolutionNames = {{512,"Низкое"},{1024,"Среднее"},{2048,"Высокое"},{4096,"Ультра"}};
                QString shadowResolutionText = resolutionNames.value(shadowSamples, QString::number(shadowSamples));
                int srIndex = shadowResolution->findText(shadowResolutionText);
                if (srIndex < 0) { shadowResolution->addItem(shadowResolutionText); srIndex = shadowResolution->count() - 1; }
                shadowResolution->setCurrentIndex(srIndex);
                const int cascadeCount = readXmlSectionValue(xml, "Viewer", "ShadowCascade", "1").toInt();
                QString cascadeText = QString::number(cascadeCount);
                int cascadeIndex = cascades->findText(cascadeText);
                if (cascadeIndex < 0) { cascades->addItem(cascadeText); cascadeIndex = cascades->count() - 1; }
                cascades->setCurrentIndex(cascadeIndex);
                const int distance = readXmlSectionValue(xml, "Viewer", "ShadowDistance", "30").toInt();
                QString distanceText = QString::number(distance);
                int distanceIndex = shadowDistance->findText(distanceText);
                if (distanceIndex < 0) { shadowDistance->addItem(distanceText); distanceIndex = shadowDistance->count() - 1; }
                shadowDistance->setCurrentIndex(distanceIndex);
            };
            auto graphicsState = [=]() {
                return QStringList{
                    resolutionCombo->currentText(), QString::number(fullscreen->isChecked()),
                    QString::number(monitor->value()), QString::number(verticalSync->isChecked()),
                    QString::number(fpsLimit->isChecked()), QString::number(fpsValue->value()),
                    aaCombo->currentText(), QString::number(viewDistance->value()),
                    QString::number(shadowsEnabled->isChecked()), shadowQuality->currentText(),
                    shadowResolution->currentText(), cascades->currentText(), shadowDistance->currentText()
                };
            };
            auto graphicsBaseline = std::make_shared<QStringList>();
            auto loadGraphicsFromDisk = [=]() {
                const QString xml = readGraphicsValues();
                if (xml.isEmpty()) {
                    QMessageBox::warning(graphicsPage, "Настройки графики",
                        QString("Не удалось прочитать файл настроек:\n%1").arg(settingsXmlPath));
                    return false;
                }
                applyXmlToControls(xml);
                *graphicsBaseline = graphicsState();
                return true;
            };
            loadGraphicsFromDisk();

            graphicsHasUnsavedChanges = [graphicsState, graphicsBaseline] {
                return *graphicsBaseline != graphicsState();
            };
            reloadGraphicsSettings = [loadGraphicsFromDisk] { loadGraphicsFromDisk(); };
            saveGraphicsSettings = [=]() {
                const QString resolution = resolutionCombo->currentText();
                const QRegularExpression resolutionRx("(\\d+)\\s*[×xX]\\s*(\\d+)");
                const auto resolutionMatch = resolutionRx.match(resolution);
                if (!resolutionMatch.hasMatch()) {
                    QMessageBox::warning(graphicsPage, "Настройки графики", "Выберите корректное разрешение окна.");
                    return false;
                }
                const int samplesValue = aaCombo->currentText() == "Выключено" ? 0 : aaCombo->currentText().remove('x').toInt();
                const QMap<QString, int> presetValues = {{"Низкое",0},{"Среднее",1},{"Высокое",2},{"Ультра",3},{"Пользовательское",4}};
                const QMap<QString, int> shadowResolutionValues = {{"Низкое",512},{"Среднее",1024},{"Высокое",2048},{"Ультра",4096}};
                QHash<QString, QString> values;
                values.insert("Width", resolutionMatch.captured(1));
                values.insert("Height", resolutionMatch.captured(2));
                values.insert("FullScreen", fullscreen->isChecked() ? "1" : "0");
                values.insert("ScreenNumber", QString::number(monitor->value()));
                values.insert("VSync", verticalSync->isChecked() ? "1" : "0");
                values.insert("MaxFPS", fpsLimit->isChecked() ? QString::number(fpsValue->value()) : "0");
                values.insert("Samples", QString::number(samplesValue));
                values.insert("ViewDistance", QString::number(viewDistance->value()));
                values.insert("Shadow", shadowsEnabled->isChecked() ? "1" : "0");
                values.insert("ShadowsPreset", QString::number(presetValues.value(shadowQuality->currentText(), 4)));
                values.insert("ShadowResolution", QString::number(shadowResolutionValues.value(shadowResolution->currentText(), shadowResolution->currentText().toInt())));
                values.insert("ShadowCascade", QString::number(cascades->currentText().toInt()));
                values.insert("ShadowDistance", QString::number(shadowDistance->currentText().toInt()));
                QString error;
                if (!writeViewerSettings(settingsXmlPath, values, &error)) {
                    QMessageBox::critical(graphicsPage, "Ошибка сохранения настроек", error);
                    return false;
                }
                *graphicsBaseline = graphicsState();
                return true;
            };
            resetCurrentSettings = [] {};
            applyCurrentSettings = [thisSave = saveGraphicsSettings] { thisSave(); };
            section = graphicsPage;
        } else if (i == 4) {
            // Сетевые настройки: подразделы оформлены так же, как группы на странице графики.
            auto *networkPage = new QWidget;
            networkPage->setObjectName("networkPage");
            networkPage->setStyleSheet(R"(
                QWidget#networkPage { background:#202c39; border:0; }
                QScrollArea { background:#202c39; border:0; }
                QScrollArea > QWidget > QWidget { background:#202c39; }
                QWidget#networkBody { background:#202c39; border:0; }
                QGroupBox {
                    background-color:#2b3949;
                    border:1px solid #4b6279;
                    border-radius:8px;
                    margin-top:20px;
                    padding:13px;
                    color:#e7edf5;
                }
                QGroupBox::title {
                    subcontrol-origin:margin;
                    subcontrol-position:top left;
                    left:13px;
                    top:-2px;
                    padding:0 6px;
                    color:#e7edf5;
                    background:transparent;
                }
                QLabel { background:transparent; color:#e7edf5; border:0; }
                QComboBox { background:#202c39; color:#e7edf5; border:1px solid #4b6279; border-radius:5px; padding:5px 8px; }
                QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
                QComboBox::drop-down:hover { background:transparent; }
                QComboBox::down-arrow { image:none; width:0px; height:0px; }
                QComboBox::drop-down:hover { background:transparent; }
                QCheckBox { background:transparent; color:#e7edf5; spacing:8px; }
                QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
                QCheckBox::indicator:checked { background:#202c39; border:1px solid #75baf0; }
                QCheckBox::indicator:unchecked:hover { border:1px solid #9bcdf4; background:#30465c; }
                QScrollBar:vertical { background:transparent; width:10px; margin:3px 1px 3px 3px; border-radius:5px; }
                QScrollBar::handle:vertical { background:#526b84; min-height:32px; border-radius:5px; }
                QScrollBar::handle:vertical:hover { background:#7298bc; }
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
                QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
                QScrollBar:horizontal { background:transparent; height:10px; border-radius:5px; }
                QScrollBar::handle:horizontal { background:#526b84; min-width:32px; border-radius:5px; }
                QScrollBar::handle:horizontal:hover { background:#7298bc; }
                QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
            )");

            auto *networkLayout = new QVBoxLayout(networkPage);
            networkLayout->setContentsMargins(0, 0, 0, 0);
            networkLayout->setSpacing(10);

            auto *networkScroll = new QScrollArea;
            networkScroll->setWidgetResizable(true);
            networkScroll->setFrameShape(QFrame::NoFrame);
            networkScroll->setAttribute(Qt::WA_StyledBackground);
            networkScroll->setAutoFillBackground(true);
            networkScroll->viewport()->setAutoFillBackground(true);
            networkScroll->viewport()->setStyleSheet("background:#202c39; border:0;");

            auto *networkBody = new QWidget;
            networkBody->setObjectName("networkBody");
            auto *networkBodyLayout = new QVBoxLayout(networkBody);
            networkBodyLayout->setContentsMargins(2, 2, 8, 2);
            networkBodyLayout->setSpacing(18);

            auto createNetworkPanel = [networkBodyLayout](const QString &title, const QString &objectName) {
                auto *group = new QGroupBox(title);
                group->setObjectName(objectName);
                group->setAttribute(Qt::WA_StyledBackground, true);
                group->setAutoFillBackground(true);
                group->setStyleSheet(R"(
                    QGroupBox {
                        background-color:#2b3949;
                        border:1px solid #4b6279;
                        border-radius:8px;
                        margin-top:20px;
                        padding:13px;
                        color:#e7edf5;
                    }
                    QGroupBox::title {
                        subcontrol-origin:margin;
                        subcontrol-position:top left;
                        left:13px;
                        top:-2px;
                        padding:0 6px;
                        color:#e7edf5;
                        background:transparent;
                    }
                    QLabel, QCheckBox { background:transparent; color:#e7edf5; border:0; }
                )");
                auto *groupLayout = new QVBoxLayout(group);
                groupLayout->setContentsMargins(8, 8, 8, 8);
                groupLayout->setSpacing(10);
                networkBodyLayout->addWidget(group);
                return group;
            };

            auto *updatesGroup = createNetworkPanel("Обновления", "networkUpdatesPanel");
            auto *updatesLayout = qobject_cast<QVBoxLayout *>(updatesGroup->layout());
            auto *autoUpdates = new GraphicsCheckBox("Проверять наличие обновлений автоматически");
            autoUpdates->setChecked(true);
            updatesLayout->addWidget(autoUpdates);

            auto *downloadGroup = createNetworkPanel("Загрузка", "networkDownloadPanel");
            auto *downloadLayout = qobject_cast<QVBoxLayout *>(downloadGroup->layout());
            auto *downloadRow = new QHBoxLayout;
            downloadRow->setSpacing(14);
            auto *downloadLabel = new QLabel("Сервер загрузки");
            auto *downloadServer = new GraphicsComboBox;
            downloadServer->addItems({"Определять автоматически", "Yandex", "Mail.ru", "Google"});
            downloadServer->setCurrentIndex(0);
            downloadServer->setMinimumHeight(34);
            // Источник выбранного сервера загрузки — RM_launcher.xml.
            {
                QFile configFile(launcherConfigPath);
                QString configXml;
                if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    configXml = QString::fromUtf8(configFile.readAll());
                    configFile.close();
                }
                const QString serverCode = readXmlSectionValue(
                    configXml, "Launcher_settings", "DownloadServer", "automatic").trimmed().toLower();
                int serverIndex = 0;
                if (serverCode == "yandex") serverIndex = 1;
                else if (serverCode == "mail") serverIndex = 2;
                else if (serverCode == "google") serverIndex = 3;
                downloadServer->setCurrentIndex(serverIndex);
            }
            downloadRow->addWidget(downloadLabel);
            downloadRow->addStretch(1);
            downloadRow->addWidget(downloadServer);
            downloadLayout->addLayout(downloadRow);

            auto *multiplayerGroup = createNetworkPanel("Мультиплеер", "networkMultiplayerPanel");
            auto *multiplayerLayout = qobject_cast<QVBoxLayout *>(multiplayerGroup->layout());
            auto *multiplayerDescription = new QLabel("Параметры сетевого подключения и взаимодействия с игровыми серверами.");
            multiplayerDescription->setWordWrap(true);
            multiplayerDescription->setObjectName("muted");
            multiplayerLayout->addWidget(multiplayerDescription);

            networkBodyLayout->addStretch(1);
            networkScroll->setWidget(networkBody);
            networkLayout->addWidget(networkScroll, 1);

            resetCurrentSettings = [autoUpdates, downloadServer] {
                autoUpdates->setChecked(true);
                downloadServer->setCurrentIndex(0);
            };
            applyCurrentSettings = [autoUpdates, downloadServer, launcherConfigPath, downloadGroup] {
                QSettings settings("RRS", "Launcher");
                settings.setValue("network/autoUpdates", autoUpdates->isChecked());

                static const QStringList serverCodes = {"automatic", "yandex", "mail", "google"};
                const int index = qBound(0, downloadServer->currentIndex(), serverCodes.size() - 1);
                QHash<QString, QString> values;
                values.insert("DownloadServer", serverCodes.at(index));
                QString error;
                if (!writeLauncherSettings(launcherConfigPath, values, &error))
                    QMessageBox::warning(downloadGroup, "Настройки Launcher", error);
            };
            section = networkPage;
        } else {
            section = new QWidget;
            auto *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setContentsMargins(0, 0, 0, 0);
            auto *description = new QLabel(settingsDescriptions[i]);
            description->setObjectName("muted");
            description->setAlignment(Qt::AlignCenter);
            sectionLayout->addStretch(1);
            sectionLayout->addWidget(description);
            sectionLayout->addStretch(1);
            resetCurrentSettings = [] {};
            applyCurrentSettings = [] {};
        }
        settingsStack->addWidget(section);
        resetActions.append(resetCurrentSettings);
        applyActions.append(applyCurrentSettings);
        // Обработчик перехода подключается ниже, когда готов общий контроль изменений.
        if (i == 0) nav->setChecked(true);
    }
    // Объединяем параметры запуска, язык и оформление в единую страницу «Главное».
    // Сами панели остаются теми же, поэтому сохраняются их стили и обработчики.
    if (settingsStack->count() >= 5) {
        QWidget *mainPage = settingsStack->widget(0);
        QWidget *languageWrapper = settingsStack->widget(1);
        QWidget *appearanceWrapper = settingsStack->widget(2);
        auto *mainLayout = qobject_cast<QVBoxLayout *>(mainPage->layout());
        auto *languagePanel = languageWrapper->findChild<QGroupBox *>("languagePanel");
        auto *appearancePanel = appearanceWrapper->findChild<QGroupBox *>("appearanceLauncherPanel");
        if (mainLayout) {
            const int stretchIndex = mainLayout->count() - 1;
            if (languagePanel) {
                languagePanel->setParent(mainPage);
                mainLayout->insertWidget(qMax(0, stretchIndex), languagePanel);
            }
            if (appearancePanel) {
                appearancePanel->setParent(mainPage);
                mainLayout->insertWidget(qMax(0, stretchIndex + (languagePanel ? 1 : 0)), appearancePanel);
            }
        }
        // Сохраняем внутренние индексы стека, чтобы пункты «Графика» и «Сеть»
        // продолжали открывать правильные страницы; сами пункты «Язык» и «Внешний вид» скрываем.
        if (settingsButtonGroup->button(1)) settingsButtonGroup->button(1)->hide();
        if (settingsButtonGroup->button(2)) settingsButtonGroup->button(2)->hide();

        // Общие кнопки «Применить» и «Сбросить» на странице «Главное» обслуживают все три панели.
        const auto launchReset = resetActions.value(0);
        const auto languageReset = resetActions.value(1);
        const auto appearanceReset = resetActions.value(2);
        resetActions[0] = [launchReset, languageReset, appearanceReset] {
            if (launchReset) launchReset();
            if (languageReset) languageReset();
            if (appearanceReset) appearanceReset();
        };
        const auto launchApply = applyActions.value(0);
        const auto languageApply = applyActions.value(1);
        const auto appearanceApply = applyActions.value(2);
        applyActions[0] = [launchApply, languageApply, appearanceApply] {
            if (launchApply) launchApply();
            if (languageApply) languageApply();
            if (appearanceApply) appearanceApply();
        };
    }

    // Единый контроль несохранённых изменений для всех вкладок настроек.
    // Снимок хранит значения элементов управления, а не только графической страницы.
    auto widgetValue = [](QWidget *w) -> QVariant {
        if (auto *v = qobject_cast<QCheckBox *>(w)) return v->isChecked();
        if (auto *v = qobject_cast<QComboBox *>(w)) return v->currentIndex();
        if (auto *v = qobject_cast<QSpinBox *>(w)) return v->value();
        if (auto *v = qobject_cast<QDoubleSpinBox *>(w)) return v->value();
        if (auto *v = qobject_cast<QLineEdit *>(w)) return v->text();
        if (auto *v = qobject_cast<QDateEdit *>(w)) return v->date();
        if (auto *v = qobject_cast<QTimeEdit *>(w)) return v->time();
        if (auto *v = qobject_cast<QSlider *>(w)) return v->value();
        return {};
    };
    auto setWidgetValue = [](QWidget *w, const QVariant &value) {
        if (auto *v = qobject_cast<QCheckBox *>(w)) v->setChecked(value.toBool());
        else if (auto *v = qobject_cast<QComboBox *>(w)) v->setCurrentIndex(value.toInt());
        else if (auto *v = qobject_cast<QSpinBox *>(w)) v->setValue(value.toInt());
        else if (auto *v = qobject_cast<QDoubleSpinBox *>(w)) v->setValue(value.toDouble());
        else if (auto *v = qobject_cast<QLineEdit *>(w)) v->setText(value.toString());
        else if (auto *v = qobject_cast<QDateEdit *>(w)) v->setDate(value.toDate());
        else if (auto *v = qobject_cast<QTimeEdit *>(w)) v->setTime(value.toTime());
        else if (auto *v = qobject_cast<QSlider *>(w)) v->setValue(value.toInt());
    };
    using SettingsSnapshot = QHash<QWidget *, QVariant>;
    auto captureSettings = [settingsStack, widgetValue]() {
        SettingsSnapshot snapshot;
        const auto widgets = settingsStack->findChildren<QWidget *>();
        for (QWidget *w : widgets) {
            const QVariant value = widgetValue(w);
            if (value.isValid()) snapshot.insert(w, value);
        }
        return snapshot;
    };
    auto settingsBaseline = std::make_shared<SettingsSnapshot>(captureSettings());
    auto settingsHaveUnsavedChanges = [captureSettings, settingsBaseline]() {
        return captureSettings() != *settingsBaseline;
    };
    auto discardAllSettings = [settingsStack, settingsBaseline, captureSettings, setWidgetValue, &reloadGraphicsSettings]() {
        for (auto it = settingsBaseline->cbegin(); it != settingsBaseline->cend(); ++it)
            if (it.key()) setWidgetValue(it.key(), it.value());
        if (reloadGraphicsSettings) reloadGraphicsSettings();
        *settingsBaseline = captureSettings();
    };
    auto applyAllSettings = [applyActions, settingsBaseline, captureSettings,
                             graphicsHasUnsavedChanges, saveGraphicsSettings]() {
        // Страница «Главное» уже объединяет настройки запуска, языка и оформления.
        // Графику сохраняем отдельно, чтобы не потерять результат проверки записи XML.
        if (graphicsHasUnsavedChanges() && !saveGraphicsSettings()) return false;
        if (applyActions.size() > 0 && applyActions[0]) applyActions[0]();
        if (applyActions.size() > 4 && applyActions[4]) applyActions[4]();
        *settingsBaseline = captureSettings();
        return true;
    };
    auto confirmSettingsLeave = [&window, settingsHaveUnsavedChanges, applyAllSettings, discardAllSettings]() {
        if (!settingsHaveUnsavedChanges()) return true;
        QMessageBox prompt(&window);
        prompt.setWindowTitle("Несохранённые изменения");
        prompt.setText("Есть несохранённые изменения. Сохранить их перед переходом?");
        prompt.setIcon(QMessageBox::Warning);
        auto *save = prompt.addButton("Сохранить", QMessageBox::AcceptRole);
        auto *discard = prompt.addButton("Не сохранять", QMessageBox::DestructiveRole);
        prompt.addButton("Отмена", QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() == save) {
            return applyAllSettings();
        }
        if (prompt.clickedButton() == discard) {
            discardAllSettings();
            return true;
        }
        return false;
    };

    for (int i = 0; i < settingsButtonGroup->buttons().size(); ++i) {
        auto *navButton = qobject_cast<QPushButton *>(settingsButtonGroup->button(i));
        if (!navButton) continue;
        QObject::connect(navButton, &QPushButton::clicked, settingsStack,
                         [settingsStack, settingsButtonGroup, navButton, i, confirmSettingsLeave]() {
            const int current = settingsStack->currentIndex();
            if (current != i && !confirmSettingsLeave()) {
                if (settingsButtonGroup->button(current))
                    settingsButtonGroup->button(current)->setChecked(true);
                return;
            }
            settingsStack->setCurrentIndex(i);
        });
    }

    auto *sharedButtonsRow = new QHBoxLayout;
    sharedButtonsRow->setContentsMargins(2, 8, 8, 2);
    sharedButtonsRow->setSpacing(8);
    sharedButtonsRow->addStretch(1);
    auto *applyShared = new QPushButton("Применить");
    applyShared->setObjectName("primary");
    sharedButtonsRow->addWidget(applyShared);
    settingsContentLayout->addWidget(settingsStack, 1);
    settingsContentLayout->addLayout(sharedButtonsRow);
    QObject::connect(applyShared, &QPushButton::clicked, settingsPage, [applyAllSettings] {
        applyAllSettings();
    });
    settingsLayout->addWidget(settingsNavPanel);
    settingsLayout->addWidget(settingsContentPanel, 1);
    // Порядок страниц должен совпадать с порядком кнопок главного меню.
    pages->addWidget(journalPage);
    pages->addWidget(trainsPage);
    pages->addWidget(addonsPage);
    pages->addWidget(makeSectionPage("Загрузки", "Раздел находится в разработке."));
    pages->addWidget(screenshotsPage);
    pages->addWidget(settingsPage);

    QObject::connect(loginButton, &QPushButton::clicked, &window, [&window] {
        QMessageBox::information(&window, "Учётная запись RRS",
            "Окно авторизации будет подключено после определения официального сервиса учётных записей RRS.");
    });

    for (int i = 0; i < mainMenuButtons.size(); ++i) {
        QObject::connect(mainMenuButtons[i], &QPushButton::clicked, pages, [pages, i, settingsPage, mainMenuButtons, confirmSettingsLeave] {
            if (pages->currentWidget() == settingsPage && !confirmSettingsLeave()) {
                if (mainMenuButtons.size() > 7) mainMenuButtons[7]->setChecked(true);
                return;
            }
            if (i >= 0 && i < pages->count()) pages->setCurrentIndex(i);
        });
    }

    auto updateFreeRideDescription = [detailsTitle, detailsText, scenarios,
                                      routeSelector, addedTrainsList, scenarioStartPoint,
                                      scenarioStartDirection, scenarioTrajectory,
                                      scenarioTrajectoryDirection, scenarioCoordinate,
                                      scenarioDate, scenarioTime, scenarioSystemTime]() {
        auto currentText = [](QComboBox *combo) {
            return combo->currentIndex() > 0 ? combo->currentText() : QString("не выбрано");
        };
        QListWidgetItem *item = scenarios->currentItem();
        const bool savedScenarioSelected = (item != nullptr);
        if (savedScenarioSelected) {
            detailsTitle->setText(item->text());
            const QString scenarioDescription = item->data(Qt::UserRole + 1).toString();
            detailsText->setText(scenarioDescription.isEmpty()
                ? QString("Сценарий RRS (Lua).") : scenarioDescription);
        } else {
            // В режиме свободной поездки описание не отображается.
            detailsTitle->clear();
            detailsText->clear();
        }
    };

    QObject::connect(scenarios, &QListWidget::currentItemChanged, &window,
        [updateFreeRideDescription](QListWidgetItem *, QListWidgetItem *) {
            updateFreeRideDescription();
        });

    // Любое изменение параметров свободной поездки переключает лаунчер
    // из режима сохранённого сценария в режим собственной конфигурации.
    auto switchToFreeRideMode = [scenarios, updateFreeRideDescription]() {
        if (scenarios->currentRow() >= 0) {
            const QSignalBlocker blocker(scenarios);
            scenarios->setCurrentRow(-1);
            scenarios->clearSelection();
        }
        updateFreeRideDescription();
    };

    // Точки старта читаются из topology/waypoints.conf выбранного маршрута.
    // Формат строки: название, имя траектории, код направления, координата, доп. поле.
    auto loadRouteWaypoints = [routeSelector, scenarioStartPoint, scenarioStartDirection]() {
        const QSignalBlocker pointBlocker(scenarioStartPoint);
        scenarioStartPoint->clear();
        scenarioStartPoint->addItem(QString(), QString());

        const QString routePath = routeSelector->currentData().toString();
        if (routePath.isEmpty()) return;

        const QString waypointsPath = QDir(routePath).filePath("topology/waypoints.conf");
        QFile waypointsFile(waypointsPath);
        if (!waypointsFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        const int selectedDirectionCode = scenarioStartDirection->currentIndex() == 1 ? -1 : 1;
        QTextStream stream(&waypointsFile);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
            if (line.isEmpty() || line.startsWith('#')) continue;

            const QStringList fields = line.split(QRegularExpression("\\t+"), Qt::KeepEmptyParts);
            if (fields.size() < 4) continue;

            const QString pointName = fields.at(0).trimmed();
            const QString trajectoryName = fields.at(1).trimmed();
            bool directionOk = false;
            const int directionCode = fields.at(2).trimmed().toInt(&directionOk);
            bool coordinateOk = false;
            const double coordinate = fields.at(3).trimmed().toDouble(&coordinateOk);
            if (pointName.isEmpty() || trajectoryName.isEmpty() || !directionOk || !coordinateOk)
                continue;
            if (directionCode != selectedDirectionCode) continue;

            const int row = scenarioStartPoint->count();
            scenarioStartPoint->addItem(pointName);
            scenarioStartPoint->setItemData(row, trajectoryName, Qt::UserRole);
            scenarioStartPoint->setItemData(row, directionCode, Qt::UserRole + 1);
            scenarioStartPoint->setItemData(row, coordinate, Qt::UserRole + 2);
        }
        scenarioStartPoint->setCurrentIndex(0);
    };

    // При выборе точки автоматически заполняем связанную траекторию,
    // направление на ней и координату из четвёртого столбца конфигурации.
    QObject::connect(scenarioStartPoint, QOverload<int>::of(&QComboBox::currentIndexChanged), &window,
        [routeSelector, scenarioStartPoint, scenarioTrajectory, scenarioTrajectoryDirection, scenarioCoordinate](int index) {
            if (index <= 0) return;

            const QString trajectoryName = scenarioStartPoint->itemData(index, Qt::UserRole).toString();
            const int directionCode = scenarioStartPoint->itemData(index, Qt::UserRole + 1).toInt();
            const double coordinate = scenarioStartPoint->itemData(index, Qt::UserRole + 2).toDouble();

            int trajectoryIndex = -1;
            for (int i = 1; i < scenarioTrajectory->count(); ++i) {
                const QString path = scenarioTrajectory->itemData(i).toString();
                if (QFileInfo(path).completeBaseName() == trajectoryName ||
                    scenarioTrajectory->itemText(i) == trajectoryName) {
                    trajectoryIndex = i;
                    break;
                }
            }
            if (trajectoryIndex < 0) {
                const QString routePath = routeSelector->currentData().toString();
                const QString trajectoryDirectory = QDir(routePath).filePath("topology/trajectories");
                const QString trajectoryPath = QDir(trajectoryDirectory).filePath(trajectoryName + ".traj");
                scenarioTrajectory->addItem(trajectoryName, trajectoryPath);
                trajectoryIndex = scenarioTrajectory->count() - 1;
            }
            // Автоматическая установка траектории из точки старта не должна
            // восприниматься как ручное изменение траектории пользователем.
            {
                const QSignalBlocker trajectoryBlocker(scenarioTrajectory);
                scenarioTrajectory->setCurrentIndex(trajectoryIndex);
            }
            scenarioTrajectoryDirection->setCurrentIndex(directionCode == 1 ? 1 : 2);
            scenarioCoordinate->setValue(coordinate);
            // Точка старта из waypoints.conf сама задаёт корректную координату,
            // даже если значение совпало с уже установленным и valueChanged не сработал.
            scenarioCoordinate->setProperty("coordinateConfirmed", true);
        });

    // Если пользователь вручную меняет стартовую траекторию после выбора
    // точки старта, сбрасываем точку: её координата и направление относятся
    // к прежней траектории.
    QObject::connect(scenarioTrajectory, QOverload<int>::of(&QComboBox::currentIndexChanged), &window,
        [scenarioStartPoint](int) {
            if (scenarioStartPoint->currentIndex() > 0)
                scenarioStartPoint->setCurrentIndex(0);
        });

    QObject::connect(scenarioStartDirection, QOverload<int>::of(&QComboBox::currentIndexChanged), &window,
        [loadRouteWaypoints](int) { loadRouteWaypoints(); });

    QObject::connect(addedTrainsList, &QListWidget::itemChanged, &window,
        [switchToFreeRideMode](QListWidgetItem *) { switchToFreeRideMode(); });
    QObject::connect(addedTrainsList->model(), &QAbstractItemModel::rowsInserted, &window,
        [switchToFreeRideMode](const QModelIndex &, int, int) { switchToFreeRideMode(); });
    QObject::connect(addedTrainsList->model(), &QAbstractItemModel::rowsRemoved, &window,
        [switchToFreeRideMode](const QModelIndex &, int, int) { switchToFreeRideMode(); });

    for (auto *combo : {static_cast<QComboBox *>(scenarioTrain),
                        static_cast<QComboBox *>(scenarioStartPoint), static_cast<QComboBox *>(scenarioStartDirection),
                        static_cast<QComboBox *>(scenarioTrajectory), static_cast<QComboBox *>(scenarioTrajectoryDirection)}) {
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), &window,
            [switchToFreeRideMode](int) { switchToFreeRideMode(); });
    }
    QObject::connect(scenarioCoordinate, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &window,
        [switchToFreeRideMode](double) { switchToFreeRideMode(); });
    QObject::connect(scenarioDate, &QDateEdit::dateChanged, &window,
        [switchToFreeRideMode](const QDate &) { switchToFreeRideMode(); });
    QObject::connect(scenarioTime, &QTimeEdit::timeChanged, &window,
        [switchToFreeRideMode](const QTime &) { switchToFreeRideMode(); });
    QObject::connect(scenarioSystemTime, &QCheckBox::toggled, &window,
        [switchToFreeRideMode](bool) { switchToFreeRideMode(); });

    // Загружаем реальные Lua-сценарии выбранного маршрута.
    auto loadRouteScenarios = [routeSelector, scenarios, scenarioTrajectory, updateFreeRideDescription]() {
        scenarios->clear();
        scenarioTrajectory->clear();
        scenarioTrajectory->addItem(QString(), QString());
        const QString routePath = routeSelector->currentData().toString();
        if (routePath.isEmpty()) {
            updateFreeRideDescription();
            return;
        }

        // Загружаем все файлы траекторий выбранного маршрута.
        const QString trajectoriesPath = QDir(routePath).filePath("topology/trajectories");
        const QFileInfoList trajectoryFiles = QDir(trajectoriesPath).entryInfoList(
            {"*.traj"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &trajectoryInfo : trajectoryFiles) {
            scenarioTrajectory->addItem(trajectoryInfo.completeBaseName(), trajectoryInfo.absoluteFilePath());
        }

        const QDir scenariosDir(QDir(routePath).filePath("scenarios"));
        const QFileInfoList scenarioDirs = scenariosDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);

        for (const QFileInfo &scenarioDirInfo : scenarioDirs) {
            // startup используется лаунчером для временного сценария свободной поездки
            // и не должен появляться в списке пользовательских сценариев.
            if (scenarioDirInfo.fileName().compare(QStringLiteral("startup"), Qt::CaseInsensitive) == 0)
                continue;
            const QString mainLuaPath = QDir(scenarioDirInfo.absoluteFilePath()).filePath("main.lua");
            if (!QFileInfo::exists(mainLuaPath))
                continue;

            QString displayName = scenarioDirInfo.fileName();
            QString description;
            QFile readmeFile(QDir(scenarioDirInfo.absoluteFilePath()).filePath("README.md"));
            if (readmeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString readme = QString::fromUtf8(readmeFile.readAll()).trimmed();
                const QStringList lines = readme.split(QRegularExpression("\\r?\\n"));
                if (!lines.isEmpty()) {
                    const QRegularExpression headingExpression("^\\s*#+\\s*(.+?)\\s*$");
                    const auto headingMatch = headingExpression.match(lines.first());
                    if (headingMatch.hasMatch()) {
                        displayName = headingMatch.captured(1).trimmed();
                        description = lines.mid(1).join("\n").trimmed();
                    } else {
                        description = readme;
                    }
                }
            }

            auto *item = new QListWidgetItem(displayName, scenarios);
            item->setData(Qt::UserRole, mainLuaPath);
            item->setData(Qt::UserRole + 1, description);
        }

        scenarios->setCurrentRow(-1);
        scenarios->clearSelection();
        updateFreeRideDescription();
    };

    // При смене маршрута сбрасываем конфигурацию свободной поездки:
    // параметры, выбранные для предыдущего маршрута, не должны переноситься
    // на новый маршрут. Список уже добавленных поездов также очищается.
    QObject::connect(routeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), &window,
        [loadRouteScenarios, loadRouteWaypoints, scenarioTrain, scenarioStartPoint,
         scenarioStartDirection, scenarioTrajectoryDirection, scenarioCoordinate,
         scenarioDate, scenarioTime, scenarioSystemTime, addedTrainsList,
         removeConfiguredTrainButton, updateScenarioDateTimeEnabled](int) {
            const QSignalBlocker trainBlocker(scenarioTrain);
            const QSignalBlocker pointBlocker(scenarioStartPoint);
            const QSignalBlocker directionBlocker(scenarioStartDirection);
            const QSignalBlocker trajectoryDirectionBlocker(scenarioTrajectoryDirection);
            const QSignalBlocker coordinateBlocker(scenarioCoordinate);
            const QSignalBlocker dateBlocker(scenarioDate);
            const QSignalBlocker timeBlocker(scenarioTime);
            const QSignalBlocker systemTimeBlocker(scenarioSystemTime);

            scenarioTrain->setCurrentIndex(0);
            scenarioStartDirection->setCurrentIndex(0);
            scenarioStartPoint->setCurrentIndex(0);
            scenarioTrajectoryDirection->setCurrentIndex(0);
            scenarioCoordinate->setValue(0.0);
            scenarioCoordinate->setProperty("coordinateConfirmed", false);
            scenarioDate->setDate(QDate::currentDate());
            scenarioTime->setTime(QTime::currentTime());
            scenarioSystemTime->setChecked(true);
            updateScenarioDateTimeEnabled(true);

            addedTrainsList->clear();
            removeConfiguredTrainButton->setEnabled(false);

            loadRouteScenarios();
            loadRouteWaypoints();
        });

    loadRouteWaypoints();
    updateFreeRideDescription();

    auto *serverProcess = new QProcess(&window);
    auto *viewerProcess = new QProcess(&window);
    auto *routeMapProcess = new QProcess(&window);

    const QString companionBinDirectory = QDir(simulatorRoot).filePath("bin");
    auto startCompanion = [&window, companionBinDirectory](QProcess *process,
                                                            const QString &fileName,
                                                            const QString &displayName) {
        if (process->state() != QProcess::NotRunning)
            return;
        const QString executable = QDir(companionBinDirectory).filePath(fileName);
        if (!QFileInfo::exists(executable)) {
            QMessageBox::warning(&window, "Запуск дополнительного приложения",
                QString("Не найден файл %1:\n%2").arg(displayName, executable));
            return;
        }
        process->setWorkingDirectory(companionBinDirectory);
        process->setProgram(executable);
        process->setArguments({});
        process->start();
        if (!process->waitForStarted(3000)) {
            QMessageBox::warning(&window, "Запуск дополнительного приложения",
                QString("Не удалось запустить %1:\n%2").arg(displayName, process->errorString()));
        }
    };
    QObject::connect(launchViewerAction, &QAction::triggered, &window,
        [startCompanion, viewerProcess] { startCompanion(viewerProcess, "viewer.exe", "viewer.exe"); });
    QObject::connect(launchRouteMapAction, &QAction::triggered, &window,
        [startCompanion, routeMapProcess] { startCompanion(routeMapProcess, "route-map.exe", "route-map.exe"); });

    // Вспомогательные приложения работают вместе с сервером и закрываются при его остановке.
    auto stopCompanionProcesses = [viewerProcess, routeMapProcess] {
        const auto stopProcess = [](QProcess *process) {
            if (process->state() != QProcess::NotRunning) {
                process->terminate();
                if (!process->waitForFinished(2000)) {
                    process->kill();
                    process->waitForFinished(1000);
                }
            }
        };
        stopProcess(viewerProcess);
        stopProcess(routeMapProcess);
    };

    auto setLaunchButtonStopped = [launch] {
        // Обновляем исходный текст перевода вместе с видимым текстом.
        // Иначе обработчик QEvent::Show может восстановить устаревшее
        // состояние «Остановить сервер» после возврата на главную вкладку.
        launch->setProperty("rrsSourceText", "Запустить сервер");
        launch->setText(launcherTranslation("Запустить сервер", gLauncherEnglish));
        launch->setStyleSheet("QPushButton#launchMain { background:#2878c7; color:white; border:1px solid #398bd9; border-right:0; border-top-left-radius:6px; border-bottom-left-radius:6px; border-top-right-radius:0; border-bottom-right-radius:0; font-weight:bold; padding:5px 9px; } QPushButton#launchMain:hover { background:#398bd9; border-color:#62a7e8; } QPushButton#launchMain:pressed { background:#2166a8; border-color:#398bd9; }");
    };
    QObject::connect(serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        &window, [setLaunchButtonStopped, stopCompanionProcesses, reloadSelectedJournal](int, QProcess::ExitStatus) {
            stopCompanionProcesses();
            setLaunchButtonStopped();
            // После завершения simulator.exe перечитываем выбранный журнал:
            // сервер мог дописать в него последние строки при остановке.
            reloadSelectedJournal();
        });
    QObject::connect(serverProcess, &QProcess::errorOccurred, &window,
        [&window, setLaunchButtonStopped, serverProcess](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                setLaunchButtonStopped();
                QMessageBox::critical(&window, "Запуск сервера",
                    QString("Не удалось запустить simulator.exe:\n%1").arg(serverProcess->errorString()));
            }
        });

    QObject::connect(launch, &QPushButton::clicked, &window,
        [&window, launch, serverProcess, viewerProcess, routeMapProcess, routeSelector, scenarios,
         addedTrainsList, scenarioDate, scenarioTime, scenarioSystemTime, simulatorRoot,
         setLaunchButtonStopped, stopCompanionProcesses, startCompanion, reloadSelectedJournal] {
            // Повторное нажатие останавливает уже работающий сервер.
            if (serverProcess->state() != QProcess::NotRunning) {
                stopCompanionProcesses();
                serverProcess->terminate();
                if (!serverProcess->waitForFinished(3000)) {
                    serverProcess->kill();
                    serverProcess->waitForFinished(1000);
                }
                setLaunchButtonStopped();
                reloadSelectedJournal();
                return;
            }

            const QString routePath = routeSelector->currentData().toString();
            if (routePath.isEmpty()) {
                QMessageBox::warning(&window, "Запуск сервера", "Сначала выберите маршрут.");
                return;
            }

            QListWidgetItem *scenarioItem = scenarios->currentItem();
            const bool isFreeRide = (scenarioItem == nullptr);
            QString mainLuaPath;
            QString scenarioFolderName;
            if (scenarioItem) {
                // Обычный сценарий запускается прежним способом без изменений.
                mainLuaPath = scenarioItem->data(Qt::UserRole).toString();
                scenarioFolderName = QFileInfo(mainLuaPath).dir().dirName();
                if (mainLuaPath.isEmpty() || !QFileInfo::exists(mainLuaPath)) {
                    QMessageBox::warning(&window, "Запуск сервера",
                        "Не удалось определить файл main.lua выбранного сценария.");
                    return;
                }
            } else {
                // Если сценарий не выбран, формируем временный startup/main.lua
                // из поездов, добавленных в генераторе свободной поездки.
                if (!addedTrainsList || addedTrainsList->count() == 0) {
                    QMessageBox::warning(&window, "Запуск сервера",
                        "Выберите сценарий или добавьте хотя бы один поезд в генераторе свободной поездки.");
                    return;
                }

                const QString startupDirectory = QDir(routePath).filePath("scenarios/startup");
                if (!QDir().mkpath(startupDirectory)) {
                    QMessageBox::critical(&window, "Генерация свободной поездки",
                        QString("Не удалось создать каталог временного сценария:\n%1").arg(startupDirectory));
                    return;
                }

                QString luaSource;
                QTextStream lua(&luaSource);
                lua.setLocale(QLocale::c());
                for (int i = 0; i < addedTrainsList->count(); ++i) {
                    const QListWidgetItem *item = addedTrainsList->item(i);
                    if (!item) continue;
                    const QString configPath = item->data(Qt::UserRole).toString();
                    const QString configName = QFileInfo(configPath).completeBaseName();
                    const QString trajectoryName = item->data(Qt::UserRole + 1).toString();
                    const QString directionText = item->data(Qt::UserRole + 2).toString();
                    const double coordinate = item->data(Qt::UserRole + 3).toDouble();
                    if (configName.isEmpty() || trajectoryName.isEmpty()) {
                        QMessageBox::warning(&window, "Генерация свободной поездки",
                            QString("У поезда №%1 отсутствует конфигурация или стартовая траектория.").arg(i));
                        return;
                    }
                    const int direction = directionText == QStringLiteral("Обратное") ? -1 : 1;
                    lua << "train" << i << " = TrainData.new()\n";
                    lua << "train" << i << ".name = \"" << i << "\"\n";
                    lua << "train" << i << ".config = \"" << configName << "\"\n";
                    lua << "train" << i << ".traj = \"" << trajectoryName << "\"\n";
                    lua << "train" << i << ".coord = " << QString::number(coordinate, 'f', 3) << "\n";
                    lua << "train" << i << ".dir = " << direction << "\n";
                    lua << "setTrain(train" << i << ")\n\n";
                }

                mainLuaPath = QDir(startupDirectory).filePath("main.lua");
                QSaveFile generatedScenario(mainLuaPath);
                if (!generatedScenario.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QMessageBox::critical(&window, "Генерация свободной поездки",
                        QString("Не удалось записать временный сценарий:\n%1\n%2")
                            .arg(mainLuaPath, generatedScenario.errorString()));
                    return;
                }
                const QByteArray luaBytes = luaSource.toUtf8();
                if (generatedScenario.write(luaBytes) != luaBytes.size() || !generatedScenario.commit()) {
                    QMessageBox::critical(&window, "Генерация свободной поездки",
                        QString("Не удалось сохранить временный сценарий:\n%1\n%2")
                            .arg(mainLuaPath, generatedScenario.errorString()));
                    return;
                }
                scenarioFolderName = QStringLiteral("startup");
            }

            const QString binDirectory = QDir(simulatorRoot).filePath("bin");
            const QString simulatorExecutable = QDir(binDirectory).filePath("simulator.exe");
            if (!QFileInfo::exists(simulatorExecutable)) {
                QMessageBox::critical(&window, "Запуск сервера",
                    QString("Не найден исполняемый файл:\n%1").arg(simulatorExecutable));
                return;
            }

            const QString routeFolderName = QFileInfo(routePath).fileName();
            serverProcess->setWorkingDirectory(binDirectory);
            serverProcess->setProgram(simulatorExecutable);

            QStringList simulatorArguments;
            simulatorArguments << QString("--route=%1").arg(routeFolderName);

            // Дата и время старта применяются только к сгенерированной свободной поездке.
            // При выборе обычного сценария параметр --start не передаётся.
            // При включённом системном времени симулятор использует системные часы.
            if (isFreeRide && !scenarioSystemTime->isChecked()) {
                const QDate startDate = scenarioDate->date();
                const QTime startTime = scenarioTime->time();
                const quint64 dateValue = (static_cast<quint64>(startDate.year()) << 16)
                                        | (static_cast<quint64>(startDate.month()) << 8)
                                        | static_cast<quint64>(startDate.day());
                // datetime.h хранит время в десятитысячных долях секунды от полуночи.
                const quint64 timeValue = static_cast<quint64>(startTime.msecsSinceStartOfDay()) * 10ULL;
                const quint64 simulatorStart = dateValue * 4294967296ULL + timeValue;
                simulatorArguments << QString("--start=%1").arg(QString::number(simulatorStart));
            }

            simulatorArguments << QString("--scenario=%1").arg(scenarioFolderName);
            serverProcess->setArguments(simulatorArguments);
            serverProcess->start();
            if (!serverProcess->waitForStarted(3000)) {
                // errorOccurred покажет подробную причину; здесь возвращаем кнопку в исходное состояние.
                setLaunchButtonStopped();
                return;
            }

            launch->setProperty("rrsSourceText", "Остановить сервер");
            launch->setText(launcherTranslation("Остановить сервер", gLauncherEnglish));
            launch->setStyleSheet("QPushButton#launchMain { background:#c73535; color:white; border:1px solid #e04b4b; border-right:0; border-top-left-radius:6px; border-bottom-left-radius:6px; border-top-right-radius:0; border-bottom-right-radius:0; font-weight:bold; padding:5px 9px; } QPushButton#launchMain:hover { background:#df4545; border-color:#ef6666; } QPushButton#launchMain:pressed { background:#a92b2b; border-color:#c73535; }");

            // Автозапуск дополнительных приложений читается из актуального RM_launcher.xml.
            const QString launcherXmlPath = QDir(simulatorRoot).filePath("cfg/RM_launcher.xml");
            QFile launcherConfigFile(launcherXmlPath);
            QString launcherConfigXml;
            if (launcherConfigFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                launcherConfigXml = QString::fromUtf8(launcherConfigFile.readAll());
                launcherConfigFile.close();
                if (readXmlSectionValue(launcherConfigXml, "Launcher_settings", "AutoStartViewer", "1") == "1")
                    startCompanion(viewerProcess, "viewer.exe", "viewer.exe");
                if (readXmlSectionValue(launcherConfigXml, "Launcher_settings", "AutoStartRouteMap", "1") == "1")
                    startCompanion(routeMapProcess, "route-map.exe", "route-map.exe");
            } else {
                QMessageBox::warning(&window, "Настройки Launcher",
                    QString("Не удалось прочитать конфигурацию автозапуска:\n%1").arg(launcherXmlPath));
            }
        });

    window.setCentralWidget(root);
    if (interfaceLanguageCombo) {
        QObject::connect(interfaceLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         &window, [&window](int index) { applyLauncherLanguage(&window, index == 1); });
        applyLauncherLanguage(&window, interfaceLanguageCombo->currentIndex() == 1);
    }
    const QString simulatorPath = QDir(QCoreApplication::applicationDirPath())
                                      .filePath("bin/simulator.exe");
    const QString simulatorVersion = simulatorProductVersion(simulatorPath);
    window.statusBar()->showMessage(
        QStringLiteral("Russian Railway Simulator · Версия %1").arg(simulatorVersion));
    window.show();
    return app.exec();
}
