<?php

/**
 * @file
 * Contains \Drupal\islandora\Form\IslandoraObjectPropertiesForm.
 */

namespace Drupal\islandora\Form;

use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Render\Element;

use AbstractObject;

class IslandoraObjectPropertiesForm extends FormBase {

  /**
   * {@inheritdoc}
   */
  public function getFormId() {
    return 'islandora_object_properties_form';
  }

  public function buildForm(array $form, FormStateInterface $form_state, AbstractObject $object = NULL) {
    $form_state->set(['object'], $object);
    $temp = islandora_invoke_hook_list(ISLANDORA_UPDATE_RELATED_OBJECTS_PROPERTIES_HOOK, $object->models, [
      $object
      ]);
    $related_objects_pids = [];
    if (!empty($temp)) {
      $related_objects_pids = array_merge_recursive($related_objects_pids, $temp);
    }
    $regenerate_derivatives_access = FALSE;
    if (islandora_object_access(ISLANDORA_REGENERATE_DERIVATIVES, $object)) {
      module_load_include('inc', 'islandora', 'includes/derivatives');
      $hooks = islandora_invoke_hook_list(ISLANDORA_DERIVATIVE_CREATION_HOOK, $object->models, [
        $object
        ]);
      $hooks = islandora_filter_derivatives($hooks, ['force' => TRUE], $object);
      if (count($hooks) >= 1) {
        $regenerate_derivatives_access = TRUE;
      }
    }
    return [
      'pid' => [
        '#type' => 'hidden',
        '#value' => $object->id,
      ],
      'object_label' => [
        '#title' => t('Item Label'),
        '#default_value' => $object->label,
        '#required' => 'TRUE',
        '#description' => t('A human-readable label'),
        // Double the normal length.
      '#size' => 120,
        // Max length for a Fedora Label.
      '#maxlength' => 255,
        '#type' => 'textfield',
      ],
      // @todo Make this into an autocomplete field that list the users in the
      // system as well.
    'object_owner' => [
        '#title' => t('Owner'),
        '#default_value' => $object->owner,
        '#required' => FALSE,
        '#description' => t("The owner's account name"),
        '#type' => 'textfield',
      ],
      'object_state' => [
        '#title' => t('State'),
        '#default_value' => $object->state,
        '#required' => TRUE,
        '#description' => t("The object's state (active, inactive or deleted)"),
        '#type' => 'select',
        '#options' => [
          'A' => 'Active',
          'I' => 'Inactive',
          'D' => 'Deleted',
        ],
      ],
      'propogate' => [
        '#title' => t('Apply changes to related objects?'),
        '#default_value' => TRUE,
        '#description' => t("Changes to owner and state will applied to associated objects. ie page objects associated with a book object."),
        '#type' => 'checkbox',
        '#access' => count($related_objects_pids),
      ],
      'related_pids' => [
        '#value' => $related_objects_pids,
        '#type' => 'hidden',
        '#access' => count($related_objects_pids),
      ],
      'submit' => [
        '#type' => 'submit',
        '#value' => 'Update Properties',
      ],
      'delete' => [
        '#type' => 'submit',
        '#access' => islandora_object_access(ISLANDORA_PURGE, $object),
        '#value' => t("Permanently remove '@label' from repository", [
          '@label' => \Drupal\Component\Utility\Unicode::truncate($object->label, 32, TRUE, TRUE)
          ]),
        '#submit' => ['islandora_object_properties_form_delete'],
        '#limit_validation_errors' => [
          [
            'pid'
            ]
          ],
      ],
      'regenerate' => [
        '#type' => 'submit',
        '#access' => $regenerate_derivatives_access,
        '#value' => t("Regenerate all derivatives"),
        '#submit' => [
          'islandora_object_properties_regenerate_derivatives'
          ],
      ],
    ];
  }

  public function submitForm(array &$form, \Drupal\Core\Form\FormStateInterface $form_state) {
    $object = $form_state->get(['object']);
    $owner = $form_state->getValue(['object_owner']);
    $state = $form_state->getValue(['object_state']);
    $label = $form_state->getValue(['object_label']);
    $propogate = $form_state->getValue(['propogate']);
    $update_owners = FALSE;
    $update_states = FALSE;
    if (isset($owner) && $owner != $object->owner) {
      try {
        $object->owner = $owner;
        $update_owners = TRUE;
        drupal_set_message(t('Successfully updated owner %s', ['%s' => $owner]));
      }
      
        catch (Exception $e) {
        $form_state->setErrorByName('object_owner', t('Error updating owner %s', [
          '%s' => $e->getMessage()
          ]));
      }
    }

    if (isset($label) && $label != $object->label) {
      try {
        $object->label = $label;
        drupal_set_message(t('Successfully updated label %s', [
          '%s' => \Drupal\Component\Utility\Html::escape($label)
          ]));
      }
      
        catch (Exception $e) {
        $form_state->setErrorByName(t('Error updating label %s', ['%s' => $e->getMessage()]));
      }
    }
    if (isset($state) && $state != $object->state) {
      try {
        $object->state = $state;
        $update_states = TRUE;
        drupal_set_message(t('Successfully updated state %s', ['%s' => $state]));
      }
      
        catch (Exception $e) {
        $form_state->setErrorByName('object_state', t('Error updating state %s', [
          '%s' => $e->getMessage()
          ]));
      }
    }
    if ($propogate && ($update_states || $update_owners)) {
      $related_objects_pids = $form_state->getValue(['related_pids']);
      $batch = [
        'title' => t('Updating related objects'),
        'file' => drupal_get_path('module', 'islandora') . '/includes/object_properties.form.inc',
        'operations' => [],
      ];

      foreach ($related_objects_pids as $pid) {
        $batch['operations'][] = [
          'islandora_update_object_properties',
          [
            $pid,
            $update_states,
            $state,
            $update_owners,
            $owner,
          ],
        ];
      }
      batch_set($batch);
    }
  }

}
?>
