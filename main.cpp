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
#include <QSaveFile>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QStackedWidget>
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

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_2.h>
#endif




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
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\\\""));
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
        const int arrowAreaWidth = 34;
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
    explicit GraphicsDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent) { setFixedWidth(500); }
protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && event->position().x() >= width() - 34) {
            event->position().y() < height() / 2.0 ? stepUp() : stepDown();
            event->accept(); return;
        }
        QDoubleSpinBox::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent *event) override
    {
        QDoubleSpinBox::paintEvent(event);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        QColor c = palette().color(QPalette::ButtonText); if (!c.isValid()) c = palette().color(QPalette::Text);
        painter.setPen(Qt::NoPen); painter.setBrush(c);
        const qreal x = width() - 14.0, y = height() / 2.0;
        QPolygonF up; up << QPointF(x-4,y-5) << QPointF(x+4,y-5) << QPointF(x,y-10);
        QPolygonF down; down << QPointF(x-4,y+5) << QPointF(x+4,y+5) << QPointF(x,y+10);
        painter.drawPolygon(up); painter.drawPolygon(down);
    }
};

class GraphicsTimeEdit : public QTimeEdit
{
public:
    explicit GraphicsTimeEdit(const QTime &time, QWidget *parent = nullptr) : QTimeEdit(time, parent) { setFixedWidth(500); }
protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && event->position().x() >= width() - 34) {
            event->position().y() < height() / 2.0 ? stepBy(1) : stepBy(-1);
            event->accept(); return;
        }
        QTimeEdit::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent *event) override
    {
        QTimeEdit::paintEvent(event);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        QColor c = palette().color(QPalette::ButtonText); if (!c.isValid()) c = palette().color(QPalette::Text);
        painter.setPen(Qt::NoPen); painter.setBrush(c);
        const qreal x = width() - 14.0, y = height() / 2.0;
        QPolygonF up; up << QPointF(x-4,y-5) << QPointF(x+4,y-5) << QPointF(x,y-10);
        QPolygonF down; down << QPointF(x-4,y+5) << QPointF(x+4,y+5) << QPointF(x,y+10);
        painter.drawPolygon(up); painter.drawPolygon(down);
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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/rrs_launcher.png"));
    app.setApplicationName("RRS Launcher");

    // В режиме разработки путь к симулятору запрашивается при каждом запуске.
    QString simulatorRoot;
    while (true) {
        const QString selected = QFileDialog::getExistingDirectory(
            nullptr, "Выберите папку симулятора RRS", QString(), QFileDialog::ShowDirsOnly);
        if (selected.isEmpty()) return 0;
        const QDir rootDir(selected);
        if (rootDir.exists("cfg/trains") && (rootDir.exists("bin/launcher.exe") || rootDir.exists("bin/display-player.exe"))) {
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
        QPushButton:pressed, QToolButton:pressed { background:#24496b; border-color:#6aa8d9; padding-top:9px; }
        QPushButton:disabled { color:#738397; background:#28313b; border-color:#39495a; }
        QPushButton#primary { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2589d5,stop:1 #1764a7); border-color:#4da5e8; color:white; font-weight:bold; }
        QPushButton#primary:hover { background:#329be8; border-color:#8bcaff; }
        QPushButton#primary:pressed { background:#15568e; }
        QPushButton#navButton { background:transparent; border:1px solid transparent; border-radius:7px; padding:9px 15px; }
        QPushButton#navButton:hover { background:#2b3b4e; border-color:#4b6680; }
        QPushButton#navButton:checked { background:#294c6b; border-color:#5aa3dc; color:white; }
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
    const QStringList mainMenuTitles = {"Главная", "Мультиплеер", "Загрузки", "Скриншоты", "Настройки"};
    QList<QPushButton*> mainMenuButtons;
    for (int i = 0; i < mainMenuTitles.size(); ++i) {
        auto *button = new QPushButton(mainMenuTitles[i]);
        button->setObjectName("navButton");
        button->setIcon(makeMenuIcon(i));
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

    // Общая плашка: навигация по разделам и запуск находятся на одной линии.
    auto *controlBar = makePanel();
    auto *controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(6, 5, 8, 5);
    controlLayout->setSpacing(8);

    auto *sectionBar = new QTabBar;
    sectionBar->setExpanding(true);
    sectionBar->setUsesScrollButtons(false);
    sectionBar->setDrawBase(false);
    sectionBar->setIconSize(QSize(18, 18));
    sectionBar->setStyleSheet(R"(
        QTabBar { background:transparent; border:0; }
        QTabBar::tab { background:transparent; color:#aab3c0; border:1px solid transparent; border-radius:6px; padding:9px 14px; margin:0 3px; min-width:90px; }
        QTabBar::tab:hover { background:#343d49; color:#e7eaf0; border-color:#536174; }
        QTabBar::tab:selected { background:#354b68; color:white; border:1px solid #5798d8; }
    )");
    sectionBar->addTab(makeHomeTabIcon(0), "Сценарии");
    sectionBar->addTab(makeHomeTabIcon(1), "Поезда");
    sectionBar->addTab(makeHomeTabIcon(2), "Дополнения");
    sectionBar->addTab(makeHomeTabIcon(3), "Журнал");
    controlLayout->addWidget(sectionBar, 1);

    auto *launchGroup = new QWidget;
    auto *launchGroupLayout = new QHBoxLayout(launchGroup);
    launchGroupLayout->setContentsMargins(0, 0, 0, 0);
    launchGroupLayout->setSpacing(0);
    auto *launch = new QPushButton("Запустить сервер");
    launch->setObjectName("launchMain");
    launch->setFixedHeight(38);
    auto *more = new QToolButton;
    more->setObjectName("launchArrow");
    more->setText(QString());
    more->setToolButtonStyle(Qt::ToolButtonTextOnly);
    more->setPopupMode(QToolButton::InstantPopup);
    more->setFixedSize(34, 38);
    more->setStyleSheet("QToolButton#launchArrow { background:#2878c7; color:white; border:1px solid #398bd9; border-left:0; border-top-right-radius:6px; border-bottom-right-radius:6px; border-top-left-radius:0; border-bottom-left-radius:0; padding:0; text-align:right; } QToolButton#launchArrow::menu-indicator { subcontrol-origin: padding; subcontrol-position: right bottom; right:8px; bottom:5px; } QToolButton#launchArrow:hover { background:#398bd9; border-color:#62a7e8; } QToolButton#launchArrow:pressed { background:#2166a8; border-color:#398bd9; }");
    launch->setStyleSheet("QPushButton#launchMain { background:#2878c7; color:white; border:1px solid #398bd9; border-right:0; border-top-left-radius:6px; border-bottom-left-radius:6px; border-top-right-radius:0; border-bottom-right-radius:0; font-weight:bold; padding:8px 13px; } QPushButton#launchMain:hover { background:#398bd9; border-color:#62a7e8; } QPushButton#launchMain:pressed { background:#2166a8; border-color:#398bd9; }");
    auto *launchMenu = new QMenu(more);
    launchMenu->addAction("Запустить Viewer", [&window] {
        QMessageBox::information(&window, "Viewer", "Запуск Viewer будет подключён позже.");
    });
    launchMenu->addAction("Запустить карту", [&window] {
        QMessageBox::information(&window, "Карта", "Запуск Route-map будет подключён позже.");
    });
    more->setMenu(launchMenu);
    launchGroupLayout->addWidget(launch);
    launchGroupLayout->addWidget(more);
    controlLayout->addWidget(launchGroup);
    workLayout->addWidget(controlBar);

    auto *sectionPages = new QStackedWidget;
    sectionPages->setStyleSheet("QStackedWidget { background:transparent; border:0; }");
    auto *scenarioPage = new QWidget;
    scenarioPage->setStyleSheet("QWidget { background:transparent; }");
    auto *scenarioLayout = new QHBoxLayout(scenarioPage);
    scenarioLayout->setContentsMargins(4, 4, 4, 4);
    scenarioLayout->setSpacing(18);

    // Список сценариев и описание размещены непосредственно на странице,
    // без отдельных карточек/плашек.
    auto *scenarioListLayout = new QVBoxLayout;
    scenarioListLayout->setContentsMargins(4, 6, 4, 6);
    auto *scenarios = new QListWidget;
    scenarios->setStyleSheet(R"(
        QListWidget { background:#303640; border:1px solid #465363; border-radius:6px; }
        QListWidget::item { background:transparent; color:#b8c9df; padding:12px 10px; margin:3px; border:0; border-radius:5px; }
        QListWidget::item:hover { background:rgba(155,188,229,24); }
        QListWidget::item:selected { background:#354b68; color:white; }
    )");
    scenarios->addItems({"Пассажирский рейс", "Грузовой поезд", "Маневровая работа"});
    scenarios->setCurrentRow(0);
    scenarioListLayout->addWidget(scenarios, 1);
    auto *scenarioButtons = new QHBoxLayout;
    scenarioButtons->setSpacing(10);
    const QString scenarioActionButtonStyle = R"(
        QPushButton#scenarioActionButton {
            background:#343d49;
            color:#e7eaf0;
            border:1px solid #586779;
            border-radius:6px;
            padding:8px 14px;
            min-height:18px;
        }
        QPushButton#scenarioActionButton:hover {
            background:#3d4a5a;
            border:1px solid #7895b8;
        }
        QPushButton#scenarioActionButton:pressed {
            background:#354b68;
            color:white;
            border:1px solid #7895b8;
        }
        QPushButton#scenarioActionButton:focus {
            outline:0;
            border:1px solid #7895b8;
        }
    )";
    auto *createScenarioButton = new QPushButton("＋ Создать");
    auto *editScenarioButton = new QPushButton("Изменить");
    auto *deleteScenarioButton = new QPushButton("Удалить");
    for (auto *button : {createScenarioButton, editScenarioButton, deleteScenarioButton}) {
        button->setObjectName("scenarioActionButton");
        button->setStyleSheet(scenarioActionButtonStyle);
        button->setCursor(Qt::PointingHandCursor);
        scenarioButtons->addWidget(button);
    }
    scenarioListLayout->addLayout(scenarioButtons);

    auto *detailsLayout = new QVBoxLayout;
    detailsLayout->setContentsMargins(8, 8, 8, 8);
    auto *detailsTitle = heading("Пассажирский рейс");
    detailsTitle->setStyleSheet("background:transparent;");
    detailsLayout->addWidget(detailsTitle);
    auto *detailsText = new QLabel("Маршрут: демонстрационный\nПоезд: не выбран\nОтправление: не задано\nПогода: по умолчанию\n\nПараметры будут загружаться из сценария RRS после интеграции.");
    detailsText->setWordWrap(true);
    detailsText->setObjectName("muted");
    detailsText->setStyleSheet("background:transparent;");
    detailsLayout->addWidget(detailsText);
    detailsLayout->addStretch();
    // Увеличенная область описания; список и ряд кнопок занимают меньшую ширину.
    scenarioLayout->addLayout(scenarioListLayout, 2);
    scenarioLayout->addLayout(detailsLayout, 3);
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

    auto [createdTrainsPanel, createdTrainsLayout] = makeTrainPanel("🚆  Созданные поезда");
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
                auto *thumbnailImage = new QLabel(card);
                thumbnailImage->setFixedSize(132, 88);
                thumbnailImage->setAlignment(Qt::AlignCenter);
                thumbnailImage->setStyleSheet("background:#202a35; border:1px solid #52657d; border-radius:5px;");

                auto &imageCache = GltfPreviewWidget::thumbnailCache();
                auto &pending = GltfPreviewWidget::pendingThumbnails();
                const auto cachedImage = imageCache.constFind(thumbnailKey);
                if (cachedImage != imageCache.constEnd()) {
                    thumbnailImage->setPixmap(*cachedImage);
                    cardLayout->addWidget(thumbnailImage, 0, Qt::AlignHCenter);
                } else if (pending.contains(thumbnailKey)) {
                    pending[thumbnailKey].append(QPointer<QLabel>(thumbnailImage));
                    cardLayout->addWidget(thumbnailImage, 0, Qt::AlignHCenter);
                } else {
                    pending.insert(thumbnailKey, {QPointer<QLabel>(thumbnailImage)});
                    auto *renderer = new GltfPreviewWidget(card);
                    renderer->setThumbnailMode();
                    cardLayout->addWidget(renderer, 0, Qt::AlignHCenter);
                    // Для каждой уникальной модели создаётся только один OpenGL-рендерер.
                    // После первого кадра его изображение становится общим для всех карточек.
                    QObject::connect(renderer, &QOpenGLWidget::frameSwapped, renderer,
                        [renderer, thumbnailKey, thumbnailImage, cardLayout]() {
                            QPointer<GltfPreviewWidget> safeRenderer(renderer);
                            QPointer<QLabel> safeImage(thumbnailImage);
                            // Захватываем кадр с небольшой задержкой: сразу после frameSwapped
                            // некоторые драйверы ещё возвращают пустой framebuffer.
                            auto capture = std::make_shared<std::function<void(int)>>();
                            *capture = [safeRenderer, safeImage, thumbnailKey, cardLayout, capture](int attempt) {
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
                                    cardLayout->removeWidget(safeRenderer);
                                    safeRenderer->hide();
                                    safeRenderer->deleteLater();
                                    cardLayout->addWidget(safeImage, 0, Qt::AlignHCenter);
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

    sectionPages->addWidget(trainsPage);

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
    sectionPages->addWidget(addonsPage);

    // Раздел журналов: выбор типа журнала через раскрывающийся список.
    auto *journalPage = new QWidget;
    journalPage->setStyleSheet("QWidget { background:transparent; }");
    auto *journalLayout = new QVBoxLayout(journalPage);
    journalLayout->setContentsMargins(8, 8, 8, 8);
    journalLayout->setSpacing(12);

    auto *journalSelectorRow = new QHBoxLayout;
    auto *journalSelector = new QToolButton;
    journalSelector->setText("Журнал запусков");
    journalSelector->setToolButtonStyle(Qt::ToolButtonTextOnly);
    journalSelector->setPopupMode(QToolButton::InstantPopup);
    journalSelector->setMinimumHeight(38);
    journalSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    journalSelector->setStyleSheet("QToolButton { background:#343b46; border:1px solid #4b5563; border-radius:6px; padding:8px 30px 8px 13px; text-align:left; } QToolButton:hover { background:#414b59; border-color:#5b6675; } QToolButton:pressed { background:#3b4655; border-color:#5b6675; } QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: right center; right:10px; }");
    auto *journalMenu = new QMenu(journalSelector);
    journalMenu->setStyleSheet(
        "QMenu { background:#343b46; color:#e7eaf0; border:1px solid #586779; "
        "border-radius:6px; padding:4px; } "
        "QMenu::item { background:transparent; padding:7px 12px; border-radius:4px; } "
        "QMenu::item:selected { background:#354b68; color:white; }");
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
    const QStringList journalNames = {"Журнал запусков", "Журнал ошибок", "Системный журнал"};
    for (const QString &journalName : journalNames) {
        journalMenu->addAction(journalName, journalSelector, [journalSelector, journalContent, journalName] {
            journalSelector->setText(journalName);
            journalContent->setPlainText(
                journalName + "\nTXT-файл выбранного журнала будет отображаться в этой области.");
        });
    }
    journalSelector->setMenu(journalMenu);
    QObject::connect(journalMenu, &QMenu::aboutToShow, journalSelector, [journalMenu, journalSelector] {
        // Подгоняем выпадающий список под фактическую ширину кнопки при каждом открытии.
        journalMenu->setFixedWidth(journalSelector->width());
    });
    journalSelectorRow->addWidget(journalSelector, 1);
    journalLayout->addLayout(journalSelectorRow);

    journalLayout->addWidget(journalContent, 1);
    sectionPages->addWidget(journalPage);

    QObject::connect(sectionBar, &QTabBar::currentChanged,
                     sectionPages, &QStackedWidget::setCurrentIndex);
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

    // Реальная авторизация будет подключена вместе с сервисом учётных записей.
    // Пока официальный сервер остаётся недоступен, а пользовательские работают как прежде.
    auto updateServers = [serverList, emptyServers, deleteServer,
                          viewerButton, routeMapButton] {
        const bool hasItems = serverList->count() > 0;
        const int selectedRow = serverList->currentRow();
        const bool hasSelection = selectedRow >= 0;
        const bool isOfficial = hasSelection &&
            serverList->item(selectedRow)->data(Qt::UserRole + 1).toBool();
        const bool isAuthenticated = false;
        const bool canConnect = hasSelection && (!isOfficial || isAuthenticated);
        serverList->setVisible(hasItems);
        emptyServers->setVisible(!hasItems);
        deleteServer->setEnabled(hasSelection && !isOfficial);
        viewerButton->setEnabled(canConnect);
        routeMapButton->setEnabled(canConnect);
    };

    QObject::connect(serverList, &QListWidget::currentRowChanged, &window,
                     [updateServers](int) { updateServers(); });

    QObject::connect(viewerButton, &QPushButton::clicked, &window, [&window, serverList] {
        if (serverList->currentRow() < 0) return;
        const QString serverName = serverList->currentItem()->data(Qt::UserRole).toString();
        QMessageBox::information(&window, "Запуск Viewer",
            QString("Выбран сервер «%1».\n\nЗапуск Viewer и подключение к серверу будут реализованы на следующем этапе.").arg(serverName));
    });

    QObject::connect(routeMapButton, &QPushButton::clicked, &window, [&window, serverList] {
        if (serverList->currentRow() < 0) return;
        const QString serverName = serverList->currentItem()->data(Qt::UserRole).toString();
        QMessageBox::information(&window, "Карта маршрута",
            QString("Выбран сервер «%1».\n\nЗапуск Route-map и подключение к серверу будут реализованы на следующем этапе.").arg(serverName));
    });

    QObject::connect(addServer, &QPushButton::clicked, &window, [&window, serverList, updateServers] {
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
        const QString serverName = name->text().trimmed();
        const QString serverAddress = QString("%1:%2")
            .arg(address->text().trimmed()).arg(port->value());

        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, serverName);
        item->setData(Qt::UserRole + 1, false);
        item->setSizeHint(QSize(0, 58));
        serverList->addItem(item);

        auto *serverRow = new QWidget;
        serverRow->setAttribute(Qt::WA_StyledBackground, true);
        serverRow->setObjectName("serverEntryRow");
        serverRow->setStyleSheet(
            "QWidget#serverEntryRow { background:transparent; "
            "border:1px solid #52657d; border-radius:7px; } "
            "QWidget#serverEntryRow QLabel { background:transparent; border:0; }");
        auto *serverRowLayout = new QHBoxLayout(serverRow);
        serverRowLayout->setContentsMargins(10, 5, 10, 5);
        auto *serverInfoLayout = new QVBoxLayout;
        serverInfoLayout->setSpacing(2);
        auto *serverNameLabel = new QLabel(serverName);
        serverNameLabel->setStyleSheet("font-weight:600;");
        auto *serverAddressLabel = new QLabel(serverAddress);
        serverAddressLabel->setObjectName("muted");
        serverInfoLayout->addWidget(serverNameLabel);
        serverInfoLayout->addWidget(serverAddressLabel);
        serverRowLayout->addLayout(serverInfoLayout, 1);

        auto *serverStatusLabel = new QLabel("Не проверен");
        serverStatusLabel->setObjectName("muted");
        serverStatusLabel->setMinimumWidth(105);
        serverStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *serverPingLabel = new QLabel("Ping: —");
        serverPingLabel->setObjectName("muted");
        serverPingLabel->setMinimumWidth(82);
        serverPingLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        serverRowLayout->addWidget(serverStatusLabel);
        serverRowLayout->addWidget(serverPingLabel);
        serverList->setItemWidget(item, serverRow);

        serverList->setCurrentItem(item);
        updateServers();
    });

    QObject::connect(deleteServer, &QPushButton::clicked, &window, [&window, serverList, updateServers] {
        const int row = serverList->currentRow();
        if (row < 0 || serverList->item(row)->data(Qt::UserRole + 1).toBool()) return;
        const QString serverName = serverList->item(row)->data(Qt::UserRole).toString();
        const auto answer = QMessageBox::warning(&window, "Удаление сервера",
            QString("Удалить сервер «%1» из списка?").arg(serverName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        delete serverList->takeItem(row);
        if (serverList->count() > 0)
            serverList->setCurrentRow(qMin(row, serverList->count() - 1));
        updateServers();
    });
    updateServers();
    pages->addWidget(multiplayerPage);

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
    pages->addWidget(makeSectionPage("Загрузки", "Раздел находится в разработке."));
    pages->addWidget(makeSectionPage("Скриншоты", "Здесь будут отображаться скриншоты симулятора."));

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
        "QFrame#panel { background:#202c39; border:1px solid #40546a; border-radius:9px; }");
    auto *settingsContentLayout = new QVBoxLayout(settingsContentPanel);
    settingsContentLayout->setContentsMargins(16, 14, 16, 16);

    auto *settingsStack = new QStackedWidget;
    settingsStack->setStyleSheet("QStackedWidget { background:#202c39; border:0; }");
    std::function<void()> resetCurrentSettings;
    std::function<void()> applyCurrentSettings;
    QList<std::function<void()>> resetActions;
    QList<std::function<void()>> applyActions;
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
            autoStartViewer->setChecked(true);
            autoStartRouteMap->setChecked(true);
            launchParametersLayout->addWidget(autoStartViewer);
            launchParametersLayout->addWidget(autoStartRouteMap);
            mainSettingsLayout->addWidget(launchParameters);
            mainSettingsLayout->addStretch(1);

            resetCurrentSettings = [autoStartViewer, autoStartRouteMap] {
                autoStartViewer->setChecked(true);
                autoStartRouteMap->setChecked(true);
            };
            applyCurrentSettings = [mainSettingsPage] {
                QMessageBox::information(mainSettingsPage, "Параметры запуска",
                    "Выбранные параметры отображаются в интерфейсе. Автоматический запуск вьювера и карты маршрута будет подключён отдельно.");
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
            languageCombo->addItems({"Русский", "English"});
            languageCombo->setCurrentIndex(0);
            languageRow->addWidget(languageLabel);
            languageRow->addStretch(1);
            languageRow->addWidget(languageCombo);
            languageGroupLayout->addLayout(languageRow);
            languageLayout->addWidget(languageGroup);
            languageLayout->addStretch(1);
            resetCurrentSettings = [languageCombo] { languageCombo->setCurrentIndex(0); };
            applyCurrentSettings = [languagePage] {
                QMessageBox::information(languagePage, "Язык лаунчера", "Выбранный язык сохранён в интерфейсе. Полная локализация будет подключена позже.");
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
            applyCurrentSettings = [themeCombo, iconsCombo, &window, &app, sectionBar] {
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
                    "QPushButton#navButton { background:transparent; color:%1; border:1px solid %2; border-radius:7px; padding:9px 15px; text-align:left; }"
                    "QPushButton#navButton:hover { background:%3; color:%1; border-color:%4; }"
                    "QPushButton#navButton:checked { background:%5; color:%6; border:1px solid %7; font-weight:600; }"
                ).arg(navText, navBorder, navHover,
                     light ? "#b2c5d8" : "#4b6680",
                     navSelected, navSelectedText,
                     light ? "#8eacc9" : "#5aa3dc");
                for (QPushButton *button : window.findChildren<QPushButton*>()) {
                    if (button->objectName() == "navButton")
                        button->setStyleSheet(navCss);
                }

                // Верхние вкладки главной страницы также получают читаемый текст
                // и лёгкую окантовку в светлой теме.
                for (QTabBar *bar : window.findChildren<QTabBar*>()) {
                    if (bar == sectionBar) {
                        const QString tabText = light ? "#34465a" : "#aab3c0";
                        const QString tabHover = light ? "#e4edf6" : "#343d49";
                        const QString tabSelected = light ? "#d7e7f5" : "#354b68";
                        bar->setStyleSheet(QString(
                            "QTabBar { background:transparent; border:0; }"
                            "QTabBar::tab { background:transparent; color:%1; border:1px solid %2; border-radius:6px; padding:9px 14px; margin:0 3px; min-width:90px; }"
                            "QTabBar::tab:hover { background:%3; color:%1; border-color:%4; }"
                            "QTabBar::tab:selected { background:%5; color:%6; border:1px solid %7; }"
                        ).arg(tabText, light ? "#d3dce6" : "transparent",
                               tabHover, light ? "#b2c5d8" : "#536174",
                               tabSelected, light ? "#20364c" : "#ffffff",
                               light ? "#8eacc9" : "#5798d8"));
                    }
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
            shadowQuality->addItems({"Низкое", "Среднее", "Высокое", "Пользовательское"});
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
            shadowDistance->addItems({"25", "50", "75", "100", "125", "150", "175", "200"});
            shadowDistance->setCurrentText("100");
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

            resetCurrentSettings = [resolutionCombo, fullscreen, windowTitle, monitor, verticalSync, aaCombo, viewDistance, shadowsEnabled, shadowQuality, shadowResolution, cascades, shadowDistance] {
                resolutionCombo->setCurrentText("1280 × 720 (HD)");
                fullscreen->setChecked(false); windowTitle->setChecked(true); monitor->setValue(0);
                verticalSync->setChecked(true); aaCombo->setCurrentText("4x");
                viewDistance->setValue(2000); shadowsEnabled->setChecked(false);
                shadowQuality->setCurrentText("Пользовательское"); shadowResolution->setCurrentText("Среднее");
                cascades->setCurrentText("3"); shadowDistance->setCurrentText("100");
            };
            applyCurrentSettings = [graphicsPage] {
                QMessageBox::information(graphicsPage, "Настройки графики", "Параметры сохранены в интерфейсе. Применение к симулятору будет подключено позже.");
            };
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
            applyCurrentSettings = [networkPage] {
                QMessageBox::information(networkPage, "Сетевые настройки",
                    "Выбранные параметры отображаются в интерфейсе. Автоматическая проверка обновлений и выбор сервера загрузки будут подключены отдельно.");
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
            applyCurrentSettings = [section] {
                QMessageBox::information(section, "Настройки", "Для этого подраздела пока нет параметров для применения.");
            };
        }
        settingsStack->addWidget(section);
        resetActions.append(resetCurrentSettings);
        applyActions.append(applyCurrentSettings);
        QObject::connect(nav, &QPushButton::clicked, settingsStack, [settingsStack, i] {
            settingsStack->setCurrentIndex(i);
        });
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

    auto *sharedButtonsRow = new QHBoxLayout;
    sharedButtonsRow->setContentsMargins(2, 8, 8, 2);
    sharedButtonsRow->setSpacing(8);
    sharedButtonsRow->addStretch(1);
    auto *applyShared = new QPushButton("Применить");
    applyShared->setObjectName("primary");
    auto *resetShared = new QPushButton("Сбросить");
    sharedButtonsRow->addWidget(applyShared);
    sharedButtonsRow->addWidget(resetShared);
    settingsContentLayout->addWidget(settingsStack, 1);
    settingsContentLayout->addLayout(sharedButtonsRow);
    QObject::connect(resetShared, &QPushButton::clicked, settingsPage, [settingsStack, resetActions] {
        const int index = settingsStack->currentIndex();
        if (index >= 0 && index < resetActions.size() && resetActions[index]) resetActions[index]();
    });
    QObject::connect(applyShared, &QPushButton::clicked, settingsPage, [settingsStack, applyActions] {
        const int index = settingsStack->currentIndex();
        if (index >= 0 && index < applyActions.size() && applyActions[index]) applyActions[index]();
    });
    settingsLayout->addWidget(settingsNavPanel);
    settingsLayout->addWidget(settingsContentPanel, 1);
    pages->addWidget(settingsPage);

    QObject::connect(loginButton, &QPushButton::clicked, &window, [&window] {
        QMessageBox::information(&window, "Учётная запись RRS",
            "Окно авторизации будет подключено после определения официального сервиса учётных записей RRS.");
    });

    for (int i = 0; i < mainMenuButtons.size(); ++i) {
        QObject::connect(mainMenuButtons[i], &QPushButton::clicked, pages, [pages, i] {
            if (i >= 0 && i < pages->count()) pages->setCurrentIndex(i);
        });
    }

    QObject::connect(scenarios, &QListWidget::currentTextChanged, &window, [detailsTitle](const QString &text) {
        detailsTitle->setText(text);
    });
    auto showScenarioDialog = [&window, scenarios, detailsTitle, detailsText](bool editing) {
        QListWidgetItem *selectedItem = editing ? scenarios->currentItem() : nullptr;
        if (editing && !selectedItem) {
            QMessageBox::information(&window, "Изменение сценария", "Сначала выберите сценарий для изменения.");
            return;
        }
        const QVariantMap saved = selectedItem ? selectedItem->data(Qt::UserRole).toMap() : QVariantMap{};

        QDialog dialog(&window);
        dialog.setWindowTitle(editing ? "Изменение сценария" : "Создание сценария");
        dialog.setModal(true);
        dialog.resize(790, 470);
        dialog.setMinimumWidth(790);
        dialog.setStyleSheet(R"(
            QDialog { background:#252b34; color:#e7edf5; }
            QLabel { color:#e7edf5; background:transparent; }
            QLineEdit, QDateEdit, QTimeEdit, QDoubleSpinBox {
                background:#202c39; color:#e7edf5; border:1px solid #4b6279;
                border-radius:5px; padding:5px 8px; min-height:22px;
                selection-background-color:#2878c7;
            }
            QComboBox { background:#202c39; color:#e7edf5; border:1px solid #4b6279; border-radius:5px; padding:5px 8px; min-height:22px; }
            QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
            QComboBox::drop-down:hover { background:transparent; }
            QComboBox::down-arrow { image:none; width:0px; height:0px; }
            QDateEdit { padding:5px 34px 5px 8px; qproperty-alignment: AlignLeft; }
            QDateEdit::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:30px; border:0; background:transparent; border-top-right-radius:5px; border-bottom-right-radius:5px; }
            QDateEdit::drop-down:hover { background:transparent; }
            QDateEdit::down-arrow { image:none; width:0px; height:0px; }
            QComboBox QAbstractItemView { background:#202c39; color:#e7edf5; selection-background-color:#354b68; border:1px solid #4b6279; }
            QCheckBox { spacing:8px; color:#e7edf5; background:transparent; }
            QCheckBox::indicator { width:16px; height:16px; border:1px solid #718ba5; border-radius:4px; background:#202c39; }
            QCheckBox::indicator:checked { background:#202c39; border:1px solid #75baf0; }
            QCheckBox::indicator:checked:hover { background:#30465c; border:1px solid #b5dcff; }
            QCheckBox::indicator:unchecked:hover { border:1px solid #9bcdf4; background:#30465c; }
            QDoubleSpinBox, QTimeEdit { padding:5px 34px 5px 8px; }
            QDoubleSpinBox::up-button, QTimeEdit::up-button { subcontrol-origin:border; subcontrol-position:top right; width:30px; height:50%; margin:0; border:0; background:transparent; }
            QDoubleSpinBox::down-button, QTimeEdit::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:30px; height:50%; margin:0; border:0; background:transparent; }
            QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover, QTimeEdit::up-button:hover, QTimeEdit::down-button:hover { background:transparent; border:0; }
            QPushButton { background:#343d49; color:#e7edf5; border:1px solid #647d98; border-radius:6px; padding:9px 16px; min-width:110px; }
            QPushButton:hover { background:#3d4a5a; border-color:#7895b8; }
            QPushButton:pressed { background:#354b68; }
            QPushButton#primaryCreate { background:#2878c7; color:white; border-color:#398bd9; font-weight:bold; min-width:190px; }
            QPushButton#primaryCreate:hover { background:#398bd9; border-color:#62a7e8; }
        )");

        auto *outer = new QVBoxLayout(&dialog);
        outer->setContentsMargins(18, 14, 18, 14);
        outer->setSpacing(2);
        auto *form = new QFormLayout;
        form->setHorizontalSpacing(14); form->setVerticalSpacing(5);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        auto *scenarioName = new QLineEdit;
        scenarioName->setPlaceholderText("Введите название сценария...");
        scenarioName->setFixedWidth(500); scenarioName->setMinimumHeight(34);
        auto makeScenarioCombo = [](const QStringList &items) {
            auto *combo = new MainSettingsComboBox;
            combo->addItems(items);
            combo->setFixedWidth(500);
            combo->setMinimumHeight(34);
            return combo;
        };
        auto *route = makeScenarioCombo({"Выберите маршрут...", "Маршрут №1", "Маршрут №2", "Маршрут №3"});
        auto *train = makeScenarioCombo({"Выберите поезд...", "Пассажирский поезд", "Грузовой поезд", "Маневровый локомотив"});
        auto *startPoint = makeScenarioCombo({"Выберите точку старта...", "Станция отправления", "Парк отправления", "Пункт оборота"});
        auto *startDirection = makeScenarioCombo({"Выберите направление...", "Прямое", "Обратное"});
        auto *trajectory = makeScenarioCombo({"Выберите стартовую траекторию...", "Главный путь", "Боковой путь", "Соединительный путь"});
        auto *trajectoryDirection = makeScenarioCombo({"Выберите направление...", "Прямое", "Обратное"});
        auto *coordinate = new GraphicsDoubleSpinBox;
        coordinate->setRange(-1000000000.0, 1000000000.0); coordinate->setDecimals(3);
        coordinate->setValue(0.0); coordinate->setSingleStep(1.0);
        coordinate->setFixedWidth(500); coordinate->setMinimumHeight(34);
        auto *date = new GraphicsDateEdit(QDate::currentDate());
        date->setCalendarPopup(true); date->setDisplayFormat("dd.MM.yyyy");
        date->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        date->setFixedWidth(500); date->setMinimumHeight(34);
        auto *time = new GraphicsTimeEdit(QTime::currentTime());
        time->setDisplayFormat("HH:mm:ss"); time->setFixedWidth(500); time->setMinimumHeight(34);
        form->addRow("Название сценария:", scenarioName);
        form->addRow("Маршрут:", route);
        form->addRow("Поезд:", train);
        form->addRow("Точка старта:", startPoint);
        form->addRow("Направление на точке старта:", startDirection);
        form->addRow("Стартовая траектория:", trajectory);
        form->addRow("Направление на стартовой траектории:", trajectoryDirection);
        form->addRow("Координата:", coordinate);
        form->addRow("Дата старта:", date);
        form->addRow("Время старта:", time);
        outer->addLayout(form);
        auto *systemTime = new GraphicsCheckBox("Использовать системное время");
        outer->addWidget(systemTime, 0, Qt::AlignLeft);
        QObject::connect(systemTime, &QCheckBox::toggled, &dialog, [date, time](bool checked) {
            date->setEnabled(!checked); time->setEnabled(!checked);
        });

        if (editing) {
            scenarioName->setText(saved.value("name", selectedItem->text()).toString());
            auto setCombo = [](QComboBox *combo, const QString &value) {
                int index = combo->findText(value);
                if (index >= 0) combo->setCurrentIndex(index);
            };
            setCombo(route, saved.value("route").toString());
            setCombo(train, saved.value("train").toString());
            setCombo(startPoint, saved.value("startPoint").toString());
            setCombo(startDirection, saved.value("startDirection").toString());
            setCombo(trajectory, saved.value("trajectory").toString());
            setCombo(trajectoryDirection, saved.value("trajectoryDirection").toString());
            coordinate->setValue(saved.value("coordinate", 0.0).toDouble());
            date->setDate(saved.value("date", QDate::currentDate()).toDate());
            time->setTime(saved.value("time", QTime::currentTime()).toTime());
            systemTime->setChecked(saved.value("systemTime", false).toBool());
        }

        auto *buttons = new QHBoxLayout; buttons->addStretch();
        auto *save = new QPushButton(editing ? "✓  Сохранить" : "✓  Создать сценарий");
        save->setObjectName("primaryCreate");
        auto *cancel = new QPushButton("✕  Отмена");
        buttons->addWidget(save); buttons->addWidget(cancel); outer->addLayout(buttons);
        QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        QObject::connect(save, &QPushButton::clicked, &dialog,
            [&dialog, scenarioName, route, train, startPoint, startDirection, trajectory, trajectoryDirection] {
                if (scenarioName->text().trimmed().isEmpty() || route->currentIndex() == 0 || train->currentIndex() == 0 ||
                    startPoint->currentIndex() == 0 || startDirection->currentIndex() == 0 ||
                    trajectory->currentIndex() == 0 || trajectoryDirection->currentIndex() == 0) {
                    QMessageBox::warning(&dialog, "Не заполнены поля",
                        "Укажите название сценария и выберите маршрут, поезд, точку старта, направления и стартовую траекторию.");
                    return;
                }
                dialog.accept();
            });

        if (dialog.exec() != QDialog::Accepted) return;
        QVariantMap data;
        data["name"] = scenarioName->text().trimmed();
        data["route"] = route->currentText();
        data["train"] = train->currentText();
        data["startPoint"] = startPoint->currentText();
        data["startDirection"] = startDirection->currentText();
        data["trajectory"] = trajectory->currentText();
        data["trajectoryDirection"] = trajectoryDirection->currentText();
        data["coordinate"] = coordinate->value();
        data["date"] = date->date();
        data["time"] = time->time();
        data["systemTime"] = systemTime->isChecked();

        if (editing) {
            selectedItem->setText(data.value("name").toString());
            selectedItem->setData(Qt::UserRole, data);
        } else {
            auto *item = new QListWidgetItem(data.value("name").toString());
            item->setData(Qt::UserRole, data);
            scenarios->addItem(item);
            scenarios->setCurrentItem(item);
        }
        detailsTitle->setText(data.value("name").toString());
        detailsText->setText(QString("Маршрут: %1\nПоезд: %2\nТочка старта: %3\nНаправление: %4\nСтартовая траектория: %5\nНаправление траектории: %6\nКоордината: %7\nДата старта: %8\nВремя старта: %9%10")
            .arg(data.value("route").toString(),
                 data.value("train").toString(),
                 data.value("startPoint").toString(),
                 data.value("startDirection").toString(),
                 data.value("trajectory").toString(),
                 data.value("trajectoryDirection").toString(),
                 QString::number(data.value("coordinate").toDouble(), 'f', 3),
                 data.value("date").toDate().toString("dd.MM.yyyy"),
                 data.value("time").toTime().toString("HH:mm:ss"),
                 data.value("systemTime").toBool() ? "\nИспользуется системное время" : ""));
    };

    QObject::connect(createScenarioButton, &QPushButton::clicked, &window,
        [&showScenarioDialog] { showScenarioDialog(false); });
    QObject::connect(editScenarioButton, &QPushButton::clicked, &window,
        [&showScenarioDialog] { showScenarioDialog(true); });
    QObject::connect(deleteScenarioButton, &QPushButton::clicked, &window,
        [&window, scenarios, detailsTitle, detailsText] {
            auto *item = scenarios->currentItem();
            if (!item) {
                QMessageBox::information(&window, "Удаление сценария", "Сначала выберите сценарий для удаления.");
                return;
            }
            const QString name = item->text();
            const auto answer = QMessageBox::question(&window, "Удаление сценария",
                QString("Удалить сценарий «%1»?").arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) return;
            delete scenarios->takeItem(scenarios->row(item));
            if (scenarios->currentItem()) {
                detailsTitle->setText(scenarios->currentItem()->text());
                const auto data = scenarios->currentItem()->data(Qt::UserRole).toMap();
                detailsText->setText(QString("Поезд: %1\nТочка старта: %2\nНаправление: %3\nСтартовая траектория: %4\nНаправление траектории: %5")
                    .arg(data.value("train", "—").toString(), data.value("startPoint", "—").toString(),
                         data.value("startDirection", "—").toString(), data.value("trajectory", "—").toString(),
                         data.value("trajectoryDirection", "—").toString()));
            } else {
                detailsTitle->setText("Описание сценария");
                detailsText->setText("Выберите сценарий из списка.");
            }
        });
    QObject::connect(launch, &QPushButton::clicked, &window, [&window] {
        QMessageBox::information(&window, "Запуск сервера",
            "Пока это демонстрационный интерфейс. Запуск сервера будет добавлен после изучения RRS.");
    });

    window.setCentralWidget(root);
    window.statusBar()->showMessage("Готово · Демонстрационный режим — интеграция с RRS пока не подключена");
    window.show();
    return app.exec();
}
