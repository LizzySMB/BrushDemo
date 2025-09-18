#include "canvas2d.h"
#include <QPainter>
#include <QMessageBox>
#include <QFileDialog>
#include <iostream>
#include "settings.h"
#include <random>

std::vector<RGBA> next_smudge_val;
std::vector<bool> visited; //for fill tool to save memory

/**
 * @brief Initializes new 500x500 canvas
 */
void Canvas2D::init() {
    setMouseTracking(true);
    m_width = 500;
    m_height = 500;
    clearCanvas();
}

/**
 * @brief Canvas2D::clearCanvas sets all canvas pixels to blank white
 */
void Canvas2D::clearCanvas() {
    m_data.assign(m_width * m_height, RGBA{255, 255, 255, 255});
    settings.imagePath = "";
    displayImage();
}

/**
 * @brief Stores the image specified from the input file in this class's
 * `std::vector<RGBA> m_image`.
 * Also saves the image width and height to canvas width and height respectively.
 * @param file: file path to an image
 * @return True if successfully loads image, False otherwise.
 */
bool Canvas2D::loadImageFromFile(const QString &file) {
    QImage myImage;
    if (!myImage.load(file)) {
        std::cout<<"Failed to load in image"<<std::endl;
        return false;
    }
    myImage = myImage.convertToFormat(QImage::Format_RGBX8888);
    m_width = myImage.width();
    m_height = myImage.height();
    QByteArray arr = QByteArray::fromRawData((const char*) myImage.bits(), myImage.sizeInBytes());

    m_data.clear();
    m_data.reserve(m_width * m_height);
    for (int i = 0; i < arr.size() / 4; i++){
        m_data.push_back(RGBA{(std::uint8_t) arr[4*i], (std::uint8_t) arr[4*i+1], (std::uint8_t) arr[4*i+2], (std::uint8_t) arr[4*i+3]});
    }
    displayImage();
    return true;
}

/**
 * @brief Saves the current canvas image to the specified file path.
 * @param file: file path to save image to
 * @return True if successfully saves image, False otherwise.
 */
bool Canvas2D::saveImageToFile(const QString &file) {
    QImage myImage = QImage(m_width, m_height, QImage::Format_RGBX8888);
    for (int i = 0; i < m_data.size(); i++){
        myImage.setPixelColor(i % m_width, i / m_width, QColor(m_data[i].r, m_data[i].g, m_data[i].b, m_data[i].a));
    }
    if (!myImage.save(file)) {
        std::cout<<"Failed to save image"<<std::endl;
        return false;
    }
    return true;
}


/**
 * @brief Get Canvas2D's image data and display this to the GUI
 */
void Canvas2D::displayImage() {
    QByteArray img(reinterpret_cast<const char *>(m_data.data()), 4 * m_data.size());
    QImage now = QImage((const uchar*)img.data(), m_width, m_height, QImage::Format_RGBX8888);
    setPixmap(QPixmap::fromImage(now));
    setFixedSize(m_width, m_height);
    update();
}

/**
 * @brief Canvas2D::resize resizes canvas to new width and height
 * @param w
 * @param h
 */
void Canvas2D::resize(int w, int h) {
    m_width = w;
    m_height = h;
    m_data.resize(w * h);
    displayImage();
}

/**
 * @brief Called when the filter button is pressed in the UI
 */
void Canvas2D::filterImage() {
    // Filter TODO: apply the currently selected filter to the loaded image

}

/**
 * @brief Called when any of the parameters in the UI are modified.
 */
void Canvas2D::settingsChanged() {
    // this saves your UI settings locally to load next time you run the program
    settings.saveSettings();

    // TODO: fill in what you need to do when brush or filter parameters change
    settings.loadSettingsOrDefaults();
}

/**
 * @brief These functions are called when the mouse is clicked and dragged on the canvas
 */
void Canvas2D::mouseDown(int x, int y) {
    // Brush TODO
    m_isDown = true;
    if (in_bounds(x,y) && settings.brushType != BRUSH_SMUDGE && settings.brushRadius != 0) {
        calibrate_mask(x,y);
    }
    else if (settings.brushType == BRUSH_SMUDGE && in_bounds(x,y) && settings.brushRadius != 0) {
        next_smudge_val.clear();
        int r = settings.brushRadius;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx*dx + dy*dy <= r*r && in_bounds(x + dx, y + dy)) {
                    next_smudge_val.push_back(m_data[row_col_to_ind(x + dx, y + dy)]);
                } else {
                    next_smudge_val.push_back({255, 255, 255, 255});
                }
            }
        }
    }
    displayImage();
}

void Canvas2D::mouseDragged(int x, int y) {
    // Brush TODO
    if (m_isDown && in_bounds(x,y) && settings.brushRadius != 0 && settings.brushType != BRUSH_FILL) {
        calibrate_mask(x,y);
        displayImage();
    }
}

void Canvas2D::mouseUp(int x, int y) {
    // Brush TODO
    next_smudge_val.clear();
    m_isDown = false;
    displayImage();
}


/**
 * @brief Canvas2D::row_col_to_ind Helper method for the many x, y to index conversions
 * @param col
 * @param row
 * @return the index form of a canvas coordinate
 */
int Canvas2D::row_col_to_ind(int col, int row) {
    return m_width * row + col;
}

/**
 * @brief Canvas2D::in_bounds helper method to check if a coordinate is in bounds of the canvas
 * @param x
 * @param y
 * @return True if the coordinates are in bounds, false otherwise
 */
bool Canvas2D::in_bounds(int x, int y) {
    return (x > 0 && x < m_width && y > 0 && y < m_height);
}

/**
 * @brief merge_colors method to blend colors already on canvas with new colors (relevant for linear, quadratic, and low-alpha strokes)
 * @param prev - value already on canvas
 * @param new_val - current brush color
 * @param opacity - opacity of current brush/its pixel at this location
 * @return
 */
RGBA merge_colors(RGBA &prev, RGBA &new_val, float opacity) {
    float alpha = new_val.a/255.0f;
    opacity = opacity * alpha;
    float merged_opacity = 1.0f - opacity;

    return {
        (std::uint8_t)(new_val.r * opacity + prev.r * merged_opacity + 0.5f),
        (std::uint8_t)(new_val.g * opacity + prev.g * merged_opacity + 0.5f),
        (std::uint8_t)(new_val.b * opacity + prev.b * merged_opacity + 0.5f),
        new_val.a
    };
}


/**
 * @brief Canvas2D::calibrate_mask method called when the mouse is down/dragged to add the brush dot/stroke there based on
 * radius and brush type set
 * @param cx - brush x centerpoint
 * @param cy - brush y centerpoint
 */
void Canvas2D::calibrate_mask(int cx, int cy) {
    int r = settings.brushRadius;
    if (settings.brushType == BRUSH_CONSTANT) {
        //circle equation (x - cx)^2 + (y - cy)^2 = r^2, going until r^2
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= r * r && in_bounds(x, y)) {
                    RGBA prev_color = m_data[row_col_to_ind(x, y)];
                    RGBA new_color = settings.brushColor;
                    m_data[row_col_to_ind(x,y)] = merge_colors(prev_color, new_color, 1);
                }
            }
        }
    }

    if (settings.brushType == BRUSH_LINEAR) {
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= r * r && in_bounds(x, y)) {
                    int r_sqr = dx * dx + dy * dy;
                    float dist = std::sqrt(r_sqr);

                    //linear decrement from 100% opacity
                    float opacity = 1.0f - dist/r;
                    RGBA prev_color = m_data[row_col_to_ind(x, y)];
                    RGBA new_color = settings.brushColor;

                    m_data[row_col_to_ind(x, y)] = merge_colors(prev_color, new_color, opacity);
                }
            }
        }
    }

    if (settings.brushType == BRUSH_QUADRATIC) {
        RGBA prev_color;
        RGBA new_color;
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= r * r && in_bounds(x, y)) {
                    int r_sqr = dx * dx + dy * dy;
                    float dist = std::sqrt(r_sqr);

                    //quadratic decrement from 100% opacity: C = 1, A = 1/r, B = -1/r
                    float opacity = 1.0f - (2 * dist)/r + (dist*dist)/(r*r);

                    prev_color = m_data[row_col_to_ind(x, y)];
                    new_color = settings.brushColor;
                    m_data[row_col_to_ind(x, y)] = merge_colors(prev_color, new_color, opacity);
                }
            }
        }
    }

    if (settings.brushType == BRUSH_SMUDGE) {
        int r = settings.brushRadius;
        int i = 0;

        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int x = cx + dx;
                int y = cy + dy;

                if (dx * dx + dy * dy <= r * r && in_bounds(x, y)) {
                    float dist = std::sqrt(dx * dx + dy * dy);

                    //quadratic here as well
                    float opacity = 1.0f - (2 * dist)/r + (dist*dist)/(r*r);

                    if (i < next_smudge_val.size()) {
                        RGBA& curr = m_data[row_col_to_ind(x, y)];
                        RGBA& smudge_color = next_smudge_val[i];
                        curr = merge_colors(curr, smudge_color, opacity);
                    }
                }
                ++i;
            }
        }

        //"picking up" the new colors
        next_smudge_val.clear();
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int x = cx + dx;
                int y = cy + dy;

                if (dx * dx + dy * dy <= r * r && in_bounds(x, y)) {
                    next_smudge_val.push_back(m_data[row_col_to_ind(x, y)]);
                } else {
                    next_smudge_val.push_back({255, 255, 255, 255});
                }
            }
        }
    }

    if (settings.brushType == BRUSH_SPRAY) {
        float probability = settings.brushDensity/100.0;
        //looked up c++ random library tool--didn't copy the code, just using the types
        std::random_device rand;
        std::mt19937 gen(rand());
        std::uniform_real_distribution<> rand_distrib(0.0,1.0);
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= r * r && in_bounds(x, y) && rand_distrib(gen) < probability) {
                    RGBA prev_color = m_data[row_col_to_ind(x, y)];
                    RGBA new_color = settings.brushColor;
                    m_data[row_col_to_ind(x,y)] = merge_colors(prev_color, new_color, 1);
                }
            }
        }
    }

    if (settings.brushType == BRUSH_FILL) {
        //checking if color is already there
        RGBA targetColor = m_data[row_col_to_ind(cx, cy)];
        if (targetColor.r == settings.brushColor.r &&
            targetColor.g == settings.brushColor.g &&
            targetColor.b == settings.brushColor.b &&
            targetColor.a == settings.brushColor.a) {
            return;
        }

        std::queue<std::pair<int, int>> q;
        q.push({cx, cy});
        visited = std::vector<bool>(m_width * m_height, false);


        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            int index = row_col_to_ind(x, y);
            if (visited[index]) continue;

            RGBA current = m_data[index];

            if (!((int)targetColor.r == (int)current.r && (int)targetColor.g == (int)current.g
                                                                                       && (int)targetColor.b == (int)current.b && (int)targetColor.a == (int)current.a)) {
                continue;
            }

            m_data[index] = settings.brushColor;
            visited[index] = true;

            // Adding nearby pixels
            if (x > 0)         q.push({x - 1, y});
            if (x < m_width-1) q.push({x + 1, y});
            if (y > 0)         q.push({x, y - 1});
            if (y < m_height-1)q.push({x, y + 1});
        }
    }
}


