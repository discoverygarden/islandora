<?php

namespace Drupal\islandora_audio\Plugin\Field\FieldFormatter;

use Drupal\islandora\Plugin\Field\FieldFormatter\IslandoraFileMediaFormatterBase;

/**
 * Plugin implementation of the 'file_audio' formatter.
 *
 * @FieldFormatter(
 *   id = "islandora_file_audio",
 *   label = @Translation("Audio with Captions"),
 *   description = @Translation("Display the file using an HTML5 audio tag."),
 *   field_types = {
 *     "file"
 *   }
 * )
 */
class IslandoraFileAudioFormatter extends IslandoraFileMediaFormatterBase {

  /**
   * {@inheritdoc}
   */
  public static function getMediaType() {
    return 'audio';
  }

}
