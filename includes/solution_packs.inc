<?php

/**
 * @file
 * Admin and callback functions for solution pack management.
 */

use Drupal\Core\Extension\InfoParser;
use Drupal\Core\Url;
use Drupal\Core\Link;
use Drupal\Component\Utility\Xss;

/**
 * Get the information about required object.
 *
 * @param string $module
 *   An optional string, identifying a module for which to get the required
 *   object information.
 *
 * @return array
 *   An associative array of info describing the required objects. If $module
 *   is not provided (or is NULL), then we provide the info for all modules. If
 *   $module is provided and we have info for the given module, only the info
 *   for that module is provided. If $module is provided and we have no info
 *   for the given module, we throw an exception.
 */
function islandora_solution_packs_get_required_objects($module = NULL) {
  // Should make this statically cache, after figuring out how exactly it
  // should be called... We occasionally load a module and attempt to install
  // it's object right away (in the same request)... This would require
  // resetting of the cache. Let's just not cache for now...
  $required_objects = [];

  if (!$required_objects) {
    $connection = islandora_get_tuque_connection();
    if (isset($module)) {
      // The module may be disabled when this function runs, as modules must be
      // disabled before they can be uninstalled. We must manually load the
      // module file to use it's islandora_required_objects hook.
      module_load_include('module', $module, $module);
      $required_objects = \Drupal::moduleHandler()->invoke($module, 'islandora_required_objects', [$connection]);
    }
    else {
      $required_objects = \Drupal::moduleHandler()->invokeAll('islandora_required_objects', [$connection]);
    }
  }

  if ($module !== NULL) {
    if (isset($required_objects[$module])) {
      return $required_objects[$module];
    }
    else {
      \Drupal::logger('islandora')->notice('Attempted to get required objects for %module... %module does not appear to have any required objects. Clear caches?', [
        '%module' => $module,
      ]);
      throw new Exception(t('Module "@module" has no required objects!', [
        '@module' => $module,
      ]));
    }
  }
  else {
    return $required_objects;
  }
}

/**
 * Install the given solution pack.
 *
 * This is to be called from the solution pack's hook_install() and
 * hook_uninstall() functions.
 *
 * It provides a convenient way to have a solution pack's required objects
 * ingested at install time.
 *
 * @param string $module
 *   The name of the module that is calling this function in its
 *   install/unistall hooks.
 * @param string $op
 *   The operation to perform, either install or uninstall.
 * @param bool $force
 *   Force the (un)installation of object.
 *
 * @todo Implement hook_modules_installed/hook_modules_uninstalled instead of
 *   calling this function directly.
 * @todo Remove the second parameter and have two seperate functions.
 */
function islandora_install_solution_pack($module, $op = 'install', $force = FALSE) {
  if ($op == 'uninstall') {
    islandora_uninstall_solution_pack($module, $force);
    return;
  }

  $t = 't';

  // Some general replacements.
  $admin_link = Link::fromTextAndUrl($t('Solution Pack admin'), Url::fromRoute('islandora.solution_packs_admin'))->toString();
  $config_link = Link::fromTextAndUrl($t('Islandora configuration'), Url::fromRoute('islandora.repository_admin'))->toString();

  $t_params = [
    '@module' => $module,
    '@config_link' => $config_link,
    '@admin_link' => $admin_link,
  ];

  module_load_include('module', 'islandora', 'islandora');
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('module', $module, $module);
  if (!islandora_describe_repository()) {
    $msg = $t('@module: Did not install any objects. Could not connect to the repository. Please check the settings on the @config_link page and install the required objects manually on the @admin_link page.', $t_params);
    drupal_set_message(Xss::filter($msg), 'error');
    return;
  }
  $required_objects = islandora_solution_packs_get_required_objects($module);
  $objects = $required_objects['objects'];
  $status_messages = [
    'up_to_date' => $t('The object already exists and is up-to-date.', $t_params),
    'missing_datastream' => $t('The object already exists but is missing a datastream. Please reinstall the object on the @admin_link page.', $t_params),
    'out_of_date' => $t('The object already exists but is out-of-date. Please update the object on the @admin_link page.', $t_params),
    'modified_datastream' => $t('The object already exists but datastreams are modified. Please reinstall the object on the @admin_link page.', $t_params),
  ];
  foreach ($objects as $object) {
    $already_exists = islandora_object_load($object->id);

    $label = $object->label;
    $object_link = Link::fromTextAndUrl($label, Url::fromRoute('islandora.view_object', ['object' => $object->id]))->toString();

    $deleted = FALSE;
    if ($already_exists) {
      if (!$force) {
        $object_status = islandora_check_object_status($object);
        $here_params = [
          '@summary' => $t("@module: Did not install @object_link.", [
            '@object_link' => $object_link,
          ] + $t_params),
          '@description' => $status_messages[$object_status['status']],
        ];
        drupal_set_message(Xss::filter(format_string('@summary @description', $here_params)), 'warning');
        continue;
      }
      else {
        $deleted = islandora_delete_object($already_exists);
      }
    }

    if ($already_exists && $deleted || !$already_exists) {
      $object = islandora_add_object($object);
      if ($object) {
        if ($deleted) {
          drupal_set_message(Xss::filter($t('@module: Successfully reinstalled. @object_link.', [
            '@object_link' => $object_link,
          ] + $t_params)), 'status');
        }
        else {
          drupal_set_message(Xss::filter($t('@module: Successfully installed. @object_link.', [
            '@object_link' => $object_link,
          ] + $t_params)), 'status');
        }
      }
      else {
        drupal_set_message($t('@module: Failed to install. @label.', [
          '@label' => $label,
        ] + $t_params), 'warning');
      }
    }
    else {
      drupal_set_message($t('@module: "@label" already exists and failed to be deleted.', [
        '@label' => $label,
      ] + $t_params), 'warning');
    }
  }
}

/**
 * Uninstalls the given solution pack.
 *
 * @param string $module
 *   The solution pack to uninstall.
 * @param bool $force
 *   Force the objects to be removed.
 *
 * @todo Implement hook_modules_uninstalled instead of calling this function
 *    directly for each solution pack.
 */
function islandora_uninstall_solution_pack($module, $force = FALSE) {
  $t = 't';
  module_load_include('module', 'islandora', 'islandora');
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('module', $module, $module);
  $config_link = Link::fromTextAndUrl($t('Islandora configuration'), Url::fromRoute('islandora.repository_admin'))->toString();

  $info_file = drupal_get_path('module', $module) . "/{$module}.info.yml";
  $parser = new InfoParser();
  $info_array = $parser->parse($info_file);
  $module_name = $info_array['name'];
  if (!islandora_describe_repository()) {
    $msg = $t('@module: Did not uninstall any objects. Could not connect to the repository. Please check the settings on the @config_link page and uninstall the required objects manually if necessary.', [
      '@module' => $module_name,
      '@config_link' => $config_link,
    ]);
    drupal_set_message(Xss::filter($msg), 'error');
    return;
  }
  $required_objects = islandora_solution_packs_get_required_objects($module);
  $objects = $required_objects['objects'];
  $filter = function ($o) {
    return islandora_object_load($o->id);
  };
  $existing_objects = array_filter($objects, $filter);

  if (!$force) {
    foreach ($existing_objects as $object) {
      $object_link = Link::createFromRoute($object->label, 'islandora.view_object', ['object' => $object->id]);
      $msg = $t('@module: Did not remove @object_link. It may be used by other sites.', [
        '@object_link' => $object_link,
        '@module' => $module_name,
      ]);

      drupal_set_message(Xss::filter($msg), 'warning');
    }
  }
  else {
    foreach ($existing_objects as $object) {
      $params = [
        '@id' => $object->id,
        '@module' => $module_name,
      ];
      islandora_delete_object($object);
      drupal_set_message($t('@module: Deleted @id.', $params));
    }
  }
}

/**
 * Function to check the status of an object against an object model array.
 *
 * @param AbstractObject $object_definition
 *   A new fedora object that defines what the object should contain.
 *
 * @return string
 *   Returns one of the following values:
 *   up_to_date, missing, missing_datastream or out_of_date
 *   You can perform an appropriate action based on this value.
 *
 * @see islandora_solution_pack_form()
 * @see islandora_install_solution_pack()
 */
function islandora_check_object_status(AbstractObject $object_definition) {
  $existing_object = islandora_object_load($object_definition->id);
  if (!$existing_object) {
    return ['status' => 'missing', 'status_friendly' => t('Missing')];
  }

  $existing_datastreams = array_keys(iterator_to_array($existing_object));
  $expected_datastreams = array_keys(iterator_to_array($object_definition));
  $datastream_diff = array_diff($expected_datastreams, $existing_datastreams);
  if (!empty($datastream_diff)) {
    $status_friendly = \Drupal::translation()->formatPlural(count($datastream_diff), 'Missing Datastream: %dsids.', 'Missing Datastreams: %dsids.', ['%dsids' => implode(', ', $datastream_diff)]);
    return [
      'status' => 'missing_datastream',
      'status_friendly' => $status_friendly,
      'data' => $datastream_diff,
    ];
  }

  $is_xml_datastream = function ($ds) {
    return $ds->mimetype == 'text/xml';
  };
  $xml_datastreams = array_filter(iterator_to_array($object_definition), $is_xml_datastream);
  $out_of_date_datastreams = [];
  foreach ($xml_datastreams as $ds) {
    $installed_version = islandora_get_islandora_datastream_version($existing_object, $ds->id);
    $available_version = islandora_get_islandora_datastream_version($object_definition, $ds->id);
    if ($available_version > $installed_version) {
      $out_of_date_datastreams[] = $ds->id;
    }
  }

  if (count($out_of_date_datastreams)) {
    $status_friendly = \Drupal::translation()->formatPlural(count($out_of_date_datastreams), 'Datastream out of date: %dsids.', 'Datastreams out of date: %dsids.', ['%dsids' => implode(', ', $out_of_date_datastreams)]);
    return [
      'status' => 'out_of_date',
      'status_friendly' => $status_friendly,
      'data' => $out_of_date_datastreams,
    ];
  }

  // This is a pretty heavy function, but I'm not sure a better way. If we have
  // performance trouble, we should maybe remove this.
  $modified_datastreams = [];
  foreach ($object_definition as $ds) {
    if ($ds->mimetype == 'text/xml'
      || $ds->mimetype == 'application/rdf+xml'
      || $ds->mimetype == 'application/xml') {
      // If the datastream is XML we use the domdocument C14N cannonicalization
      // function to test if they are equal, because the strings likely won't
      // be equal as Fedora does some XML mangling. In order for C14N to work
      // we need to replace the info:fedora namespace, as C14N hates it.
      // C14N also doesn't normalize whitespace at the end of lines and Fedora
      // will sometimes replace new-lines with white-space. So first we strip
      // leading/tailing white-space and replace all new-lines within the xml
      // document to account for Fedora's weird formatting.
      $xsl = new DOMDocument();
      $xsl->load(drupal_get_path('module', 'islandora') . '/xml/strip_newlines_and_whitespace.xsl');
      $xslt = new XSLTProcessor();
      $xslt->importStyleSheet($xsl);
      $object_definition_dom = new DOMDocument();
      $object_definition_dom->preserveWhiteSpace = FALSE;
      $object_definition_dom->loadXML(str_replace('info:', 'http://', $ds->content), LIBXML_NOWARNING);
      $object_definition_dom = $xslt->transformToDoc($object_definition_dom);
      $object_actual_dom = new DOMDocument();
      $object_actual_dom->preserveWhiteSpace = FALSE;
      $object_actual_dom->loadXML(str_replace('info:', 'http://', $existing_object[$ds->id]->content), LIBXML_NOWARNING);
      $object_actual_dom = $xslt->transformToDoc($object_actual_dom);

      // Fedora changes the xml structure so we need to cannonize it.
      if ($object_actual_dom->C14N() != $object_definition_dom->C14N()) {
        $modified_datastreams[] = $ds->id;
      }
    }
    else {
      $object_definition_hash = md5($ds->content);
      $object_actual_hash = md5($existing_object[$ds->id]->content);
      if ($object_definition_hash != $object_actual_hash) {
        $modified_datastreams[] = $ds->id;;
      }
    }
  }
  if (count($modified_datastreams)) {
    $status_friendly = \Drupal::translation()->formatPlural(count($modified_datastreams), 'Modified Datastream: %dsids.', 'Modified Datastreams: %dsids.', ['%dsids' => implode(', ', $modified_datastreams)]);
    return [
      'status' => 'modified_datastream',
      'data' => $modified_datastreams,
      'status_friendly' => $status_friendly,
    ];
  }

  // If not anything else we can assume its up to date.
  return ['status' => 'up_to_date', 'status_friendly' => t('Up-to-date')];
}

/**
 * @defgroup viewer-functions
 * @{
 * Helper functions to include viewers for solution packs.
 */

/**
 * A form construct to create a viewer selection table.
 *
 * The list of selectable viewers is limited by the $mimetype and the $model
 * parameters. When neither are given all defined viewers are listed. If only
 * $mimetype is given only viewers that support that mimetype will be listed,
 * the same goes for the $model parameter. If both are given, than any viewer
 * that supports either the give $mimetype or $model will be listed.
 *
 * @param string $variable_id
 *   The ID of the Drupal variable to save the viewer settings in.
 * @param string $mimetype
 *   The table will be populated with viewers supporting this mimetype.
 * @param string $model
 *   The table will be populated with viewers supporting this content model.
 *
 * @return array
 *   A form api array which defines a themed table to select a viewer.
 */
function islandora_viewers_form($variable_id = NULL, $mimetype = NULL, $model = NULL) {
  $form = [];
  $viewers = islandora_get_viewers($mimetype, $model);
  if (!empty($viewers)) {
    $no_viewer = [];
    $no_viewer['none'] = [
      'label' => t('None'),
      'description' => t("Don't use a viewer for this solution pack."),
    ];
    $viewers = array_merge_recursive($no_viewer, $viewers);
  }
  // @FIXME
  // The correct configuration object could not be determined. You'll need to
  // rewrite this call manually.
  // $viewers_config = variable_get($variable_id, array());
  $form['viewers'] = [
    '#type' => 'fieldset',
    '#title' => t('Viewers'),
  ];

  if (!empty($viewers)) {
    $form['viewers'][$variable_id] = [
      '#type' => 'item',
      '#title' => t('Select a viewer'),
      '#description' => t('Preferred viewer for your solution pack. These may be provided by third-party modules.'),
      '#tree' => TRUE,
      '#theme' => 'islandora_viewers_table',
    ];

    foreach ($viewers as $name => $profile) {
      $options[$name] = '';
      $form['viewers'][$variable_id]['name'][$name] = [
        '#type' => 'hidden',
        '#value' => $name,
      ];
      $form['viewers'][$variable_id]['label'][$name] = [
        '#type' => 'item',
        '#markup' => $profile['label'],
      ];
      $form['viewers'][$variable_id]['description'][$name] = [
        '#type' => 'item',
        '#markup' => $profile['description'],
      ];
      // @FIXME
      // l() expects a Url object, created from a route name or external URI.
      // @codingStandardsIgnoreStart
      // $form['viewers'][$variable_id]['configuration'][$name] = [
              // '#type' => 'item',
              // '#markup' => (isset($profile['configuration']) AND $profile['configuration'] != '') ? l(t('configure'), $profile['configuration']) : '',
              // ];
      // @codingStandardsIgnoreEnd
    }
    $form['viewers'][$variable_id]['default'] = [
      '#type' => 'radios',
      '#options' => isset($options) ? $options : [],
      // @codingStandardsIgnoreStart
      '#default_value' => !empty($viewers_config) ? $viewers_config['default'] : 'none',
      // @codingStandardsIgnoreEnd
    ];
  }
  else {
    $form['viewers'][$variable_id . '_no_viewers'] = [
      '#markup' => t('No viewers detected.'),
    ];

    // @FIXME
    // The correct configuration object could not be determined. You'll need to
    // rewrite this call manually.
    // variable_del($variable_id);
  }
  return $form;
}

/**
 * Returns all defined viewers.
 *
 * The list of selectable viewers is limited by the $mimetype and the
 * $content_model parameters. When neither are given all defined viewers are
 * listed. If only $mimetype is given only viewers that support that mimetype
 * will be listed, the same goes for the $content_model parameter. If both are
 * given, than any viewer that supports either the give $mimetype or $model will
 * be listed.
 *
 * @param array $mimetype
 *   List of mimetypes that the viewer supports.
 * @param string $content_model
 *   Specify a content model to return only viewers that support the content
 *   model.
 *
 * @return array
 *   Viewer definitions, or FALSE if none are found.
 */
function islandora_get_viewers(array $mimetype = [], $content_model = NULL) {
  $viewers = [];
  $defined_viewers = \Drupal::moduleHandler()->invokeAll('islandora_viewer_info');

  if (!is_array($mimetype)) {
    $mimetype = [$mimetype];
  }

  // Filter viewers by MIME type.
  foreach ($defined_viewers as $key => $value) {
    $value['mimetype'] = isset($value['mimetype']) ? $value['mimetype'] : [];
    $value['model'] = isset($value['model']) ? $value['model'] : [];
    if (array_intersect($mimetype, $value['mimetype']) or in_array($content_model, $value['model'])) {
      $viewers[$key] = $value;
    }
  }
  if (!empty($viewers)) {
    return $viewers;
  }
  return FALSE;
}

/**
 * Implements theme_hook().
 */
function theme_islandora_viewers_table($variables) {
  $form = $variables['form'];
  // @codingStandardsIgnoreStart
  // XXX Usage is commented out below.
  $rows = [];
  foreach ($form['name'] as $key => $element) {
    if (is_array($element) && \Drupal\Core\Render\Element::child($key)) {
      $row = [];
      $row[] = ['data' => \Drupal::service("renderer")->render($form['default'][$key])];
      $row[] = ['data' => \Drupal::service("renderer")->render($form['label'][$key])];
      $row[] = ['data' => \Drupal::service("renderer")->render($form['description'][$key])];
      $row[] = ['data' => \Drupal::service("renderer")->render($form['configuration'][$key])];
      $rows[] = ['data' => $row];
    }
  }
  // @codingStandardsIgnoreEnd
  // @codingStandardsIgnoreStart
  // XXX Usage is commented out below.
  $header = [];
  $header[] = ['data' => t('Default')];
  $header[] = ['data' => t('Label')];
  $header[] = ['data' => t('Description')];
  $header[] = ['data' => t('Configuration')];
  // @codingStandardsIgnoreEnd
  $output = '';
  // @FIXME
  // theme() has been renamed to _theme() and should NEVER be called directly.
  // Calling _theme() directly can alter the expected output and potentially
  // introduce security issues (see https://www.drupal.org/node/2195739). You
  // should use renderable arrays instead.
  //
  // Remove coding standard annotations above.
  //
  // @see https://www.drupal.org/node/2195739
  // @codingStandardsIgnoreStart
  // $output .= theme('table', array(
  //                'header' => $header,
  //                'rows' => $rows,
  //                'attributes' => array('id' => 'islandora-viewers-table'),
  //     ));
  // @codingStandardsIgnoreEnd
  $output .= drupal_render_children($form);
  return $output;
}

/**
 * Gather information and return a rendered viewer.
 *
 * @param mixed $params
 *   Array or string with data the module needs in order to render a full viewer.
 * @param string $variable_id
 *   The id of the Drupal variable the viewer settings are saved in.
 * @param AbstractObject $fedora_object
 *   The tuque object representing the fedora object being displayed.
 *
 * @return string
 *   The callback to the viewer module. Returns a rendered viewer. Returns FALSE
 *   if no viewer is set.
 */
function islandora_get_viewer($params = NULL, $variable_id = NULL, AbstractObject $fedora_object = NULL) {

  // @FIXME
  // The correct configuration object could not be determined. You'll need to
  // rewrite this call manually.
  // $settings = variable_get($variable_id, array());
  // @codingStandardsIgnoreStart
  // XXX $settings is commented out and thus undefined.
  if (!empty($settings) and $settings['default'] !== 'none') {
    $viewer_id = islandora_get_viewer_id($variable_id);
    if ($viewer_id and $params !== NULL) {
      $callback = islandora_get_viewer_callback($viewer_id);
      if (function_exists($callback)) {
        return $callback($params, $fedora_object);
      }
    }
  }
  // @codingStandardsIgnoreEnd
  return FALSE;
}

/**
 * Get id of the enabled viewer.
 *
 * @param string $variable_id
 *   The ID of the Drupal variable the viewer settings are saved in.
 *
 * @return string
 *   The enabled viewer id. Returns FALSE if no viewer config is set.
 */
function islandora_get_viewer_id($variable_id) {

  // @FIXME
  // The correct configuration object could not be determined. You'll need to
  // rewrite this call manually.
  // $viewers_config = variable_get($variable_id, array());
  // @codingStandardsIgnoreStart
  // XXX $viewers_config is undefined, commented out above.
  if (!empty($viewers_config)) {
    return $viewers_config['default'];
  }
  // @codingStandardsIgnoreEnd
  return FALSE;
}

/**
 * Get callback function for a viewer.
 *
 * @param string $viewer_id
 *   The ID of a viewer.
 *
 * @return string
 *   The callback function as a string as defined by the viewer.
 */
function islandora_get_viewer_callback($viewer_id = NULL) {
  if ($viewer_id !== NULL) {
    $viewers = \Drupal::moduleHandler()->invokeAll('islandora_viewer_info');
    if (isset($viewers[$viewer_id]['callback'])) {
      return $viewers[$viewer_id]['callback'];
    }
  }
  return FALSE;
}

/**
 * @} End of "defgroup viewer-functions".
 */
