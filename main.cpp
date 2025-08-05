#include "device/context.h"

int main()
{
    const Context context("WCR", 800, 600);
    auto image = context.create_image(
        {800, 600, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        1,
        false);
}
